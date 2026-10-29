#include "aerospace.hpp"
#include <algorithm>
#include <sstream>

namespace oxide {

AerospaceVerifier::AerospaceVerifier(const Program& program) : program_(program) {}

void AerospaceVerifier::emit_error(SourceLocation loc, const std::string& message) {
    std::ostringstream ss;
    ss << "aerospace error[" << loc.line << ":" << loc.column << "]: " << message;
    errors_.push_back(ss.str());
}

void AerospaceVerifier::collect_calls_from_expr(const std::string& caller, const Expression& expr) {
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        call_graph_[caller].insert(call->callee);
        for (const auto& arg : call->arguments) {
            collect_calls_from_expr(caller, *arg);
        }
        return;
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        collect_calls_from_expr(caller, *bin->left);
        collect_calls_from_expr(caller, *bin->right);
        return;
    }

    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        collect_calls_from_expr(caller, *un->operand);
        return;
    }

    if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        collect_calls_from_expr(caller, *assign->target);
        collect_calls_from_expr(caller, *assign->value);
        return;
    }

    if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        collect_calls_from_expr(caller, *if_expr->condition);
        collect_calls_from_block(caller, *if_expr->then_branch);
        if (if_expr->else_branch) {
            collect_calls_from_block(caller, *if_expr->else_branch);
        }
        return;
    }

    if (auto* while_expr = dynamic_cast<const WhileExpr*>(&expr)) {
        collect_calls_from_expr(caller, *while_expr->condition);
        collect_calls_from_block(caller, *while_expr->body);
        return;
    }

    if (auto* ret = dynamic_cast<const ReturnExpr*>(&expr)) {
        if (ret->value) {
            collect_calls_from_expr(caller, *ret->value);
        }
        return;
    }

    if (auto* block = dynamic_cast<const BlockExpr*>(&expr)) {
        collect_calls_from_block(caller, *block);
        return;
    }

    if (auto* s_init = dynamic_cast<const StructInitExpr*>(&expr)) {
        for (const auto& f : s_init->fields) {
            collect_calls_from_expr(caller, *f.value);
        }
        return;
    }

    if (auto* fa = dynamic_cast<const FieldAccessExpr*>(&expr)) {
        collect_calls_from_expr(caller, *fa->target);
        return;
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&expr)) {
        if (!mc->resolved_callee.empty()) {
            call_graph_[caller].insert(mc->resolved_callee);
        }
        collect_calls_from_expr(caller, *mc->receiver);
        for (const auto& arg : mc->arguments) {
            collect_calls_from_expr(caller, *arg);
        }
        return;
    }

    if (auto* ev = dynamic_cast<const EnumVariantExpr*>(&expr)) {
        if (ev->payload) {
            collect_calls_from_expr(caller, *ev->payload);
        }
        return;
    }

    if (auto* match_expr = dynamic_cast<const MatchExpr*>(&expr)) {
        collect_calls_from_expr(caller, *match_expr->target);
        for (const auto& arm : match_expr->arms) {
            collect_calls_from_expr(caller, *arm.body);
        }
        return;
    }

    if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        for (const auto& el : arr->elements) {
            collect_calls_from_expr(caller, *el);
        }
        return;
    }

    if (auto* rep = dynamic_cast<const ArrayRepeatExpr*>(&expr)) {
        collect_calls_from_expr(caller, *rep->value);
        return;
    }

    if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        collect_calls_from_expr(caller, *idx->target);
        collect_calls_from_expr(caller, *idx->index);
        return;
    }

    if (auto* range = dynamic_cast<const RangeExpr*>(&expr)) {
        collect_calls_from_expr(caller, *range->start);
        collect_calls_from_expr(caller, *range->end);
        return;
    }

    if (auto* for_expr = dynamic_cast<const ForExpr*>(&expr)) {
        collect_calls_from_expr(caller, *for_expr->iterable);
        collect_calls_from_block(caller, *for_expr->body);
        return;
    }
}

void AerospaceVerifier::collect_calls_from_block(const std::string& caller, const BlockExpr& block) {
    for (const auto& stmt : block.statements) {
        if (auto* let = dynamic_cast<const LetStmt*>(stmt.get())) {
            if (let->initializer) {
                collect_calls_from_expr(caller, *let->initializer);
            }
        } else if (auto* expr_stmt = dynamic_cast<const ExprStmt*>(stmt.get())) {
            if (expr_stmt->expr) {
                collect_calls_from_expr(caller, *expr_stmt->expr);
            }
        }
    }
    if (block.trailing_expr) {
        collect_calls_from_expr(caller, *block.trailing_expr);
    }
}

void AerospaceVerifier::build_call_graph() {
    for (const auto& fn : program_.functions) {
        func_locations_[fn->name] = fn->location;
        call_graph_[fn->name] = {};
        if (fn->body) {
            collect_calls_from_block(fn->name, *fn->body);
        }
    }
    for (const auto& ib : program_.impl_blocks) {
        for (const auto& fn : ib->methods) {
            func_locations_[fn->name] = fn->location;
            call_graph_[fn->name] = {};
            if (fn->body) {
                collect_calls_from_block(fn->name, *fn->body);
            }
        }
    }
}

void AerospaceVerifier::check_recursion() {
    std::map<std::string, int> state; // 0 = unvisited, 1 = visiting, 2 = visited
    std::set<std::string> reported_cycles;

    auto dfs = [&](auto& self, const std::string& node, std::vector<std::string>& path) -> void {
        state[node] = 1;
        path.push_back(node);

        auto it = call_graph_.find(node);
        if (it != call_graph_.end()) {
            for (const auto& neighbor : it->second) {
                if (state[neighbor] == 1) {
                    // Back-edge found: cycle starts at neighbor in current path
                    auto cycle_start = std::find(path.begin(), path.end(), neighbor);
                    std::ostringstream ss;
                    ss << "recursion detected along call chain [";
                    for (auto p = cycle_start; p != path.end(); ++p) {
                        if (p != cycle_start) ss << " -> ";
                        ss << *p;
                    }
                    ss << " -> " << neighbor << "]: violates NASA/JPL Flight Software Rule 3 (unbounded stack growth)";
                    std::string msg = ss.str();
                    if (reported_cycles.insert(msg).second) {
                        emit_error(func_locations_[neighbor], msg);
                    }
                } else if (state[neighbor] == 0) {
                    self(self, neighbor, path);
                }
            }
        }

        path.pop_back();
        state[node] = 2;
    };

    for (const auto& [name, _] : call_graph_) {
        if (state[name] == 0) {
            std::vector<std::string> path;
            dfs(dfs, name, path);
        }
    }
}

static void scan_ast_rules(const Expression& expr,
                           const std::function<void(SourceLocation, const std::string&)>& emit_err) {
    if (auto* while_expr = dynamic_cast<const WhileExpr*>(&expr)) {
        if (auto* b = dynamic_cast<const BoolLiteralExpr*>(while_expr->condition.get())) {
            if (b->value) {
                emit_err(while_expr->location,
                         "unbounded loop detected: NASA/JPL Rule 1 requires statically determinable loop bounds (infinite 'while true' loop is banned)");
            }
        }
        scan_ast_rules(*while_expr->condition, emit_err);
        if (while_expr->body) {
            for (const auto& s : while_expr->body->statements) {
                if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
                    if (let->initializer) scan_ast_rules(*let->initializer, emit_err);
                } else if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
                    if (es->expr) scan_ast_rules(*es->expr, emit_err);
                }
            }
            if (while_expr->body->trailing_expr) scan_ast_rules(*while_expr->body->trailing_expr, emit_err);
        }
        return;
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        if (call->callee == "malloc" || call->callee == "calloc" ||
            call->callee == "realloc" || call->callee == "free") {
            emit_err(call->location,
                     "dynamic memory allocation '" + call->callee + "' is banned under NASA/JPL Rule 2 in safety-critical profile");
        }
        for (const auto& a : call->arguments) scan_ast_rules(*a, emit_err);
        return;
    }

    if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        scan_ast_rules(*if_expr->condition, emit_err);
        if (if_expr->then_branch) {
            for (const auto& s : if_expr->then_branch->statements) {
                if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
                    if (let->initializer) scan_ast_rules(*let->initializer, emit_err);
                } else if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
                    if (es->expr) scan_ast_rules(*es->expr, emit_err);
                }
            }
            if (if_expr->then_branch->trailing_expr) scan_ast_rules(*if_expr->then_branch->trailing_expr, emit_err);
        }
        if (if_expr->else_branch) {
            for (const auto& s : if_expr->else_branch->statements) {
                if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
                    if (let->initializer) scan_ast_rules(*let->initializer, emit_err);
                } else if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
                    if (es->expr) scan_ast_rules(*es->expr, emit_err);
                }
            }
            if (if_expr->else_branch->trailing_expr) scan_ast_rules(*if_expr->else_branch->trailing_expr, emit_err);
        }
        return;
    }

    if (auto* for_expr = dynamic_cast<const ForExpr*>(&expr)) {
        scan_ast_rules(*for_expr->iterable, emit_err);
        if (for_expr->body) {
            for (const auto& s : for_expr->body->statements) {
                if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
                    if (let->initializer) scan_ast_rules(*let->initializer, emit_err);
                } else if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
                    if (es->expr) scan_ast_rules(*es->expr, emit_err);
                }
            }
            if (for_expr->body->trailing_expr) scan_ast_rules(*for_expr->body->trailing_expr, emit_err);
        }
        return;
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        scan_ast_rules(*bin->left, emit_err);
        scan_ast_rules(*bin->right, emit_err);
        return;
    }

    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        scan_ast_rules(*un->operand, emit_err);
        return;
    }

    if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        scan_ast_rules(*assign->target, emit_err);
        scan_ast_rules(*assign->value, emit_err);
        return;
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&expr)) {
        scan_ast_rules(*mc->receiver, emit_err);
        for (const auto& a : mc->arguments) scan_ast_rules(*a, emit_err);
        return;
    }

    if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        for (const auto& el : arr->elements) scan_ast_rules(*el, emit_err);
        return;
    }

    if (auto* rep = dynamic_cast<const ArrayRepeatExpr*>(&expr)) {
        scan_ast_rules(*rep->value, emit_err);
        return;
    }

    if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        scan_ast_rules(*idx->target, emit_err);
        scan_ast_rules(*idx->index, emit_err);
        return;
    }
}

void AerospaceVerifier::check_loop_bounds() {
    auto emit_err = [this](SourceLocation loc, const std::string& msg) {
        emit_error(loc, msg);
    };

    for (const auto& fn : program_.functions) {
        if (fn->body) {
            for (const auto& s : fn->body->statements) {
                if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
                    if (let->initializer) scan_ast_rules(*let->initializer, emit_err);
                } else if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
                    if (es->expr) scan_ast_rules(*es->expr, emit_err);
                }
            }
            if (fn->body->trailing_expr) scan_ast_rules(*fn->body->trailing_expr, emit_err);
        }
    }

    for (const auto& ib : program_.impl_blocks) {
        for (const auto& fn : ib->methods) {
            if (fn->body) {
                for (const auto& s : fn->body->statements) {
                    if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
                        if (let->initializer) scan_ast_rules(*let->initializer, emit_err);
                    } else if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
                        if (es->expr) scan_ast_rules(*es->expr, emit_err);
                    }
                }
                if (fn->body->trailing_expr) scan_ast_rules(*fn->body->trailing_expr, emit_err);
            }
        }
    }
}

void AerospaceVerifier::check_memory_allocation() {
    for (const auto& ext : program_.extern_blocks) {
        for (const auto& f : ext->declarations) {
            if (f.name == "malloc" || f.name == "calloc" || f.name == "realloc" || f.name == "free") {
                emit_error(f.location, "dynamic memory allocation '" + f.name + "' is banned under NASA/JPL Rule 2 in safety-critical profile");
            }
        }
    }
}

bool AerospaceVerifier::verify() {
    build_call_graph();
    check_recursion();
    check_loop_bounds();
    check_memory_allocation();
    return errors_.empty();
}

} // namespace oxide
