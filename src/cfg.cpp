#include "cfg.hpp"

namespace oxide {

CFGFunction CFGBuilder::build(const Function& func) {
    CFGBuilder builder(func);
    return std::move(builder.cfg_);
}

CFGBuilder::CFGBuilder(const Function& func) {
    build_cfg(func);
}

// Builds the Control Flow Graph (CFG) for an individual function.
// Translates high-level hierarchical AST structures into flat Basic Blocks
// suitable for Kildall dataflow analysis, definite initialization, and NLL borrow checking.
//
// Invariants enforced:
//   1. Each BasicBlock has at most one terminator (Branch, Goto, Return).
//   2. Statements within a basic block execute strictly sequentially without internal branches.
//   3. Predecessor and successor edge maps are bidirectionally consistent.
void CFGBuilder::build_cfg(const Function& func) {
    cfg_.name = func.name;
    cfg_.parameters = func.parameters;
    cfg_.return_type = func.return_type;
    cfg_.entry_block = cfg_.new_block();

    for (const auto& p : func.parameters) {
        cfg_.var_is_mut[p.name] = p.is_mut;
        cfg_.var_is_ref[p.name] = p.type.is_ref;
        cfg_.var_is_mut_ref[p.name] = p.type.is_mut_ref;
        cfg_.var_loc[p.name] = p.location;
    }

    BlockId current_bb = cfg_.entry_block;
    if (func.body) {
        lower_block(*func.body, current_bb);
    }

    // Ensure the final block has a terminator
    if (cfg_.blocks[current_bb].terminator.kind == TerminatorKind::None) {
        cfg_.blocks[current_bb].terminator = CFGTerminator{
            TerminatorKind::Return, "", 0, 0, 0, "", func.location
        };
    }

    // Build predecessor and successor graph
    for (auto& bb : cfg_.blocks) {
        if (bb.terminator.kind == TerminatorKind::Goto) {
            cfg_.add_edge(bb.id, bb.terminator.goto_target);
        } else if (bb.terminator.kind == TerminatorKind::Branch) {
            cfg_.add_edge(bb.id, bb.terminator.then_target);
            cfg_.add_edge(bb.id, bb.terminator.else_target);
        }
    }
}

void CFGBuilder::lower_block(const BlockExpr& block, BlockId& current_bb) {
    for (const auto& stmt : block.statements) {
        lower_statement(*stmt, current_bb);
    }
    if (block.trailing_expr) {
        lower_expression(*block.trailing_expr, current_bb);
    }
}

void CFGBuilder::lower_statement(const Statement& stmt, BlockId& current_bb) {
    if (auto* let = dynamic_cast<const LetStmt*>(&stmt)) {
        cfg_.var_is_mut[let->var_name] = let->is_mutable;
        cfg_.var_is_ref[let->var_name] = let->declared_type.is_ref;
        cfg_.var_is_mut_ref[let->var_name] = let->declared_type.is_mut_ref;
        cfg_.var_loc[let->var_name] = let->location;

        if (let->initializer) {
            if (auto* un = dynamic_cast<const UnaryExpr*>(let->initializer.get())) {
                if (un->op == UnaryOp::Ref || un->op == UnaryOp::RefMut) {
                    if (auto* var_target = dynamic_cast<const VariableExpr*>(un->operand.get())) {
                        CFGStatement s;
                        s.kind = CFGStmtKind::Borrow;
                        s.target_var = let->var_name;
                        s.borrowed_var = var_target->name;
                        s.is_mut_borrow = (un->op == UnaryOp::RefMut);
                        s.read_vars = {var_target->name};
                        s.location = let->location;
                        cfg_.blocks[current_bb].statements.push_back(std::move(s));
                        return;
                    }
                }
            }

            std::vector<std::string> reads;
            collect_read_vars(*let->initializer, reads);
            CFGStatement s;
            s.kind = CFGStmtKind::AssignVar;
            s.target_var = let->var_name;
            s.read_vars = reads;
            s.location = let->location;
            cfg_.blocks[current_bb].statements.push_back(std::move(s));
        }
    } else if (auto* expr_stmt = dynamic_cast<const ExprStmt*>(&stmt)) {
        if (expr_stmt->expr) {
            lower_expression(*expr_stmt->expr, current_bb);
        }
    }
}

void CFGBuilder::lower_expression(const Expression& expr, BlockId& current_bb) {
    if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        if (auto* var = dynamic_cast<const VariableExpr*>(assign->target.get())) {
            if (auto* un = dynamic_cast<const UnaryExpr*>(assign->value.get())) {
                if (un->op == UnaryOp::Ref || un->op == UnaryOp::RefMut) {
                    if (auto* var_target = dynamic_cast<const VariableExpr*>(un->operand.get())) {
                        CFGStatement s;
                        s.kind = CFGStmtKind::Borrow;
                        s.target_var = var->name;
                        s.borrowed_var = var_target->name;
                        s.is_mut_borrow = (un->op == UnaryOp::RefMut);
                        s.read_vars = {var_target->name};
                        s.location = assign->location;
                        cfg_.blocks[current_bb].statements.push_back(std::move(s));
                        return;
                    }
                }
            }

            std::vector<std::string> reads;
            collect_read_vars(*assign->value, reads);
            CFGStatement s;
            s.kind = CFGStmtKind::AssignVar;
            s.target_var = var->name;
            s.read_vars = reads;
            s.location = assign->location;
            cfg_.blocks[current_bb].statements.push_back(std::move(s));
            return;
        }

        if (auto* fa = dynamic_cast<const FieldAccessExpr*>(assign->target.get())) {
            std::string base_var;
            const Expression* cur = fa->target.get();
            while (auto* inner_fa = dynamic_cast<const FieldAccessExpr*>(cur)) {
                cur = inner_fa->target.get();
            }
            if (auto* var = dynamic_cast<const VariableExpr*>(cur)) {
                base_var = var->name;
            }

            std::vector<std::string> reads;
            collect_read_vars(*assign->value, reads);
            if (!base_var.empty()) {
                reads.push_back(base_var);
            }

            CFGStatement s;
            s.kind = CFGStmtKind::AssignField;
            s.target_var = base_var;
            s.field_name = fa->field_name;
            s.read_vars = reads;
            s.location = assign->location;
            cfg_.blocks[current_bb].statements.push_back(std::move(s));
            return;
        }

        if (auto* un = dynamic_cast<const UnaryExpr*>(assign->target.get())) {
            if (un->op == UnaryOp::Deref) {
                std::string base_var;
                if (auto* var = dynamic_cast<const VariableExpr*>(un->operand.get())) {
                    base_var = var->name;
                }

                std::vector<std::string> reads;
                collect_read_vars(*assign->value, reads);
                if (!base_var.empty()) {
                    reads.push_back(base_var);
                }

                CFGStatement s;
                s.kind = CFGStmtKind::AssignDeref;
                s.target_var = base_var;
                s.read_vars = reads;
                s.location = assign->location;
                cfg_.blocks[current_bb].statements.push_back(std::move(s));
                return;
            }
        }

        if (auto* idx = dynamic_cast<const IndexExpr*>(assign->target.get())) {
            std::string base_var;
            const Expression* cur = idx->target.get();
            while (auto* inner_idx = dynamic_cast<const IndexExpr*>(cur)) {
                cur = inner_idx->target.get();
            }
            if (auto* var = dynamic_cast<const VariableExpr*>(cur)) {
                base_var = var->name;
            }

            std::vector<std::string> reads;
            collect_read_vars(*assign->value, reads);
            collect_read_vars(*idx->index, reads);
            if (!base_var.empty()) {
                reads.push_back(base_var);
            }

            CFGStatement s;
            s.kind = CFGStmtKind::AssignIndex;
            s.target_var = base_var;
            s.read_vars = reads;
            s.location = assign->location;
            cfg_.blocks[current_bb].statements.push_back(std::move(s));
            return;
        }
    }

    if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        std::vector<std::string> cond_reads;
        collect_read_vars(*if_expr->condition, cond_reads);
        std::string cond_var = cond_reads.empty() ? "" : cond_reads[0];

        CFGStatement cond_stmt;
        cond_stmt.kind = CFGStmtKind::EvalExpr;
        cond_stmt.read_vars = cond_reads;
        cond_stmt.location = if_expr->condition->location;
        cfg_.blocks[current_bb].statements.push_back(std::move(cond_stmt));

        BlockId then_bb = cfg_.new_block();
        BlockId else_bb = cfg_.new_block();
        BlockId join_bb = cfg_.new_block();

        cfg_.blocks[current_bb].terminator = CFGTerminator{
            TerminatorKind::Branch, cond_var, then_bb, else_bb, 0, "", if_expr->location
        };

        BlockId then_cur = then_bb;
        if (if_expr->then_branch) {
            lower_block(*if_expr->then_branch, then_cur);
        }
        if (cfg_.blocks[then_cur].terminator.kind == TerminatorKind::None) {
            cfg_.blocks[then_cur].terminator = CFGTerminator{
                TerminatorKind::Goto, "", 0, 0, join_bb, "", if_expr->location
            };
        }

        BlockId else_cur = else_bb;
        if (if_expr->else_branch) {
            lower_block(*if_expr->else_branch, else_cur);
        }
        if (cfg_.blocks[else_cur].terminator.kind == TerminatorKind::None) {
            cfg_.blocks[else_cur].terminator = CFGTerminator{
                TerminatorKind::Goto, "", 0, 0, join_bb, "", if_expr->location
            };
        }

        current_bb = join_bb;
        return;
    }

    if (auto* while_expr = dynamic_cast<const WhileExpr*>(&expr)) {
        BlockId header_bb = cfg_.new_block();
        BlockId body_bb = cfg_.new_block();
        BlockId exit_bb = cfg_.new_block();

        cfg_.blocks[current_bb].terminator = CFGTerminator{
            TerminatorKind::Goto, "", 0, 0, header_bb, "", while_expr->location
        };

        std::vector<std::string> cond_reads;
        collect_read_vars(*while_expr->condition, cond_reads);
        std::string cond_var = cond_reads.empty() ? "" : cond_reads[0];

        CFGStatement cond_stmt;
        cond_stmt.kind = CFGStmtKind::EvalExpr;
        cond_stmt.read_vars = cond_reads;
        cond_stmt.location = while_expr->condition->location;
        cfg_.blocks[header_bb].statements.push_back(std::move(cond_stmt));

        cfg_.blocks[header_bb].terminator = CFGTerminator{
            TerminatorKind::Branch, cond_var, body_bb, exit_bb, 0, "", while_expr->location
        };

        BlockId body_cur = body_bb;
        if (while_expr->body) {
            lower_block(*while_expr->body, body_cur);
        }
        if (cfg_.blocks[body_cur].terminator.kind == TerminatorKind::None) {
            cfg_.blocks[body_cur].terminator = CFGTerminator{
                TerminatorKind::Goto, "", 0, 0, header_bb, "", while_expr->location
            };
        }

        current_bb = exit_bb;
        return;
    }

    if (auto* for_expr = dynamic_cast<const ForExpr*>(&expr)) {
        cfg_.var_is_mut[for_expr->var_name] = false;
        cfg_.var_loc[for_expr->var_name] = for_expr->location;

        BlockId header_bb = cfg_.new_block();
        BlockId body_bb = cfg_.new_block();
        BlockId exit_bb = cfg_.new_block();

        std::vector<std::string> iter_reads;
        collect_read_vars(*for_expr->iterable, iter_reads);

        cfg_.blocks[current_bb].terminator = CFGTerminator{
            TerminatorKind::Goto, "", 0, 0, header_bb, "", for_expr->location
        };

        CFGStatement cond_stmt;
        cond_stmt.kind = CFGStmtKind::EvalExpr;
        cond_stmt.read_vars = iter_reads;
        cond_stmt.location = for_expr->location;
        cfg_.blocks[header_bb].statements.push_back(std::move(cond_stmt));

        cfg_.blocks[header_bb].terminator = CFGTerminator{
            TerminatorKind::Branch, "", body_bb, exit_bb, 0, "", for_expr->location
        };

        BlockId body_cur = body_bb;
        CFGStatement loop_var_init;
        loop_var_init.kind = CFGStmtKind::AssignVar;
        loop_var_init.target_var = for_expr->var_name;
        loop_var_init.read_vars = iter_reads;
        loop_var_init.location = for_expr->location;
        cfg_.blocks[body_cur].statements.push_back(std::move(loop_var_init));

        if (for_expr->body) {
            lower_block(*for_expr->body, body_cur);
        }
        if (cfg_.blocks[body_cur].terminator.kind == TerminatorKind::None) {
            cfg_.blocks[body_cur].terminator = CFGTerminator{
                TerminatorKind::Goto, "", 0, 0, header_bb, "", for_expr->location
            };
        }

        current_bb = exit_bb;
        return;
    }

    if (auto* ret = dynamic_cast<const ReturnExpr*>(&expr)) {
        std::vector<std::string> ret_reads;
        if (ret->value) {
            collect_read_vars(*ret->value, ret_reads);
        }
        std::string ret_var = ret_reads.empty() ? "" : ret_reads[0];
        cfg_.blocks[current_bb].terminator = CFGTerminator{
            TerminatorKind::Return, "", 0, 0, 0, ret_var, ret->location
        };

        BlockId dead_bb = cfg_.new_block();
        cfg_.blocks[dead_bb].terminator = CFGTerminator{
            TerminatorKind::Return, "", 0, 0, 0, "", ret->location
        };
        current_bb = dead_bb;
        return;
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&expr)) {
        std::vector<std::string> reads;
        collect_read_vars(expr, reads);
        CFGStatement s;
        s.kind = CFGStmtKind::MethodCall;
        s.read_vars = reads;
        s.location = mc->location;
        if (auto* var = dynamic_cast<const VariableExpr*>(mc->receiver.get())) {
            s.borrowed_var = var->name;
            s.is_mut_borrow = (mc->resolved_self_kind == SelfKind::RefMut);
        }
        cfg_.blocks[current_bb].statements.push_back(std::move(s));
        return;
    }

    std::vector<std::string> reads;
    collect_read_vars(expr, reads);
    CFGStatement s;
    s.kind = CFGStmtKind::EvalExpr;
    s.read_vars = reads;
    s.location = expr.location;
    cfg_.blocks[current_bb].statements.push_back(std::move(s));
}

void CFGBuilder::collect_read_vars(const Expression& expr, std::vector<std::string>& reads) {
    if (auto* var = dynamic_cast<const VariableExpr*>(&expr)) {
        reads.push_back(var->name);
        return;
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        collect_read_vars(*bin->left, reads);
        collect_read_vars(*bin->right, reads);
        return;
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        collect_read_vars(*un->operand, reads);
        return;
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        for (const auto& a : call->arguments) {
            collect_read_vars(*a, reads);
        }
        return;
    }
    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&expr)) {
        collect_read_vars(*mc->receiver, reads);
        for (const auto& a : mc->arguments) {
            collect_read_vars(*a, reads);
        }
        return;
    }
    if (auto* fa = dynamic_cast<const FieldAccessExpr*>(&expr)) {
        collect_read_vars(*fa->target, reads);
        return;
    }
    if (auto* s_init = dynamic_cast<const StructInitExpr*>(&expr)) {
        for (const auto& f : s_init->fields) {
            collect_read_vars(*f.value, reads);
        }
        return;
    }
    if (auto* ev = dynamic_cast<const EnumVariantExpr*>(&expr)) {
        if (ev->payload) {
            collect_read_vars(*ev->payload, reads);
        }
        return;
    }
    if (auto* match_expr = dynamic_cast<const MatchExpr*>(&expr)) {
        collect_read_vars(*match_expr->target, reads);
        for (const auto& arm : match_expr->arms) {
            collect_read_vars(*arm.body, reads);
        }
        return;
    }
    if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        for (const auto& el : arr->elements) {
            collect_read_vars(*el, reads);
        }
        return;
    }
    if (auto* rep = dynamic_cast<const ArrayRepeatExpr*>(&expr)) {
        collect_read_vars(*rep->value, reads);
        return;
    }
    if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        collect_read_vars(*idx->target, reads);
        collect_read_vars(*idx->index, reads);
        return;
    }
    if (auto* range = dynamic_cast<const RangeExpr*>(&expr)) {
        collect_read_vars(*range->start, reads);
        collect_read_vars(*range->end, reads);
        return;
    }
    if (auto* for_expr = dynamic_cast<const ForExpr*>(&expr)) {
        collect_read_vars(*for_expr->iterable, reads);
        return;
    }
}

} // namespace oxide
