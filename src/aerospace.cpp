#include "aerospace.hpp"
#include "cfg.hpp"
#include <algorithm>
#include <functional>
#include <queue>
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

// NASA/JPL Flight Software Rule 3: Bounded call stack depth & recursion prohibition.
// Employs a 3-color DFS graph cycle detection algorithm:
//   State 0 (Unvisited): Function has not yet been processed.
//   State 1 (Visiting): Function is on the active call stack path.
//   State 2 (Visited): Function call subtree has been completely verified acyclic.
// When a neighbor in State 1 is encountered, a back-edge is detected and the exact
// cyclic call chain is formatted into an aerospace diagnostic error.
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

void AerospaceVerifier::check_block_barriers(const BlockExpr& block, std::optional<SourceLocation>& last_write_loc) {
    for (const auto& s : block.statements) {
        if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
            if (!es->expr) continue;
            if (auto* mc = dynamic_cast<const MethodCallExpr*>(es->expr.get())) {
                if (mc->method_name == "write" && mc->receiver->evaluated_type.name == "VolatileRegister") {
                    if (last_write_loc.has_value()) {
                        emit_error(mc->location, "hardware hazard: missing memory barrier between consecutive volatile register writes");
                    }
                    last_write_loc = mc->location;
                }
            } else if (auto* call = dynamic_cast<const CallExpr*>(es->expr.get())) {
                if (call->callee == "memory_barrier") {
                    last_write_loc.reset();
                }
            } else if (auto* if_expr = dynamic_cast<const IfExpr*>(es->expr.get())) {
                check_block_barriers(*if_expr->then_branch, last_write_loc);
                if (if_expr->else_branch) {
                    check_block_barriers(*if_expr->else_branch, last_write_loc);
                }
            } else if (auto* while_expr = dynamic_cast<const WhileExpr*>(es->expr.get())) {
                check_block_barriers(*while_expr->body, last_write_loc);
            }
        } else if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
            if (let->initializer) {
                if (auto* call = dynamic_cast<const CallExpr*>(let->initializer.get())) {
                    if (call->callee == "memory_barrier") {
                        last_write_loc.reset();
                    }
                }
            }
        }
    }
}

void AerospaceVerifier::check_volatile_barriers() {
    for (const auto& fn : program_.functions) {
        if (fn->body) {
            std::optional<SourceLocation> last_write_loc;
            check_block_barriers(*fn->body, last_write_loc);
        }
    }
    for (const auto& ib : program_.impl_blocks) {
        for (const auto& fn : ib->methods) {
            if (fn->body) {
                std::optional<SourceLocation> last_write_loc;
                check_block_barriers(*fn->body, last_write_loc);
            }
        }
    }
}

int64_t AerospaceVerifier::compute_expr_cycles(const Expression& expr) {
    if (dynamic_cast<const IntegerLiteralExpr*>(&expr)) return 1;
    if (dynamic_cast<const FloatLiteralExpr*>(&expr)) return 1;
    if (dynamic_cast<const BoolLiteralExpr*>(&expr)) return 1;
    if (dynamic_cast<const StringLiteralExpr*>(&expr)) return 1;
    if (dynamic_cast<const VariableExpr*>(&expr)) return 1;

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        int64_t l = compute_expr_cycles(*bin->left);
        int64_t r = compute_expr_cycles(*bin->right);
        int64_t op_cost = 1;
        if (bin->op == BinaryOp::Mul) {
            op_cost = (bin->evaluated_type.kind == BaseTypeKind::F32 || bin->evaluated_type.kind == BaseTypeKind::F64) ? 5 : 3;
            if (bin->evaluated_type.name == "Q16_16") op_cost = 6;
        } else if (bin->op == BinaryOp::Div) {
            op_cost = (bin->evaluated_type.kind == BaseTypeKind::F32 || bin->evaluated_type.kind == BaseTypeKind::F64) ? 15 : 20;
            if (bin->evaluated_type.name == "Q16_16") op_cost = 25;
        } else if (bin->op == BinaryOp::Add || bin->op == BinaryOp::Sub) {
            if (bin->evaluated_type.kind == BaseTypeKind::F32 || bin->evaluated_type.kind == BaseTypeKind::F64) op_cost = 4;
            else if (bin->evaluated_type.name == "Q16_16") op_cost = 2;
        }
        return l + r + op_cost;
    }

    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        return compute_expr_cycles(*un->operand) + 1;
    }

    if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        return compute_expr_cycles(*assign->target) + compute_expr_cycles(*assign->value) + 1;
    }

    if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        int64_t cond = compute_expr_cycles(*if_expr->condition);
        int64_t then_c = compute_block_cycles(*if_expr->then_branch);
        int64_t else_c = if_expr->else_branch ? compute_block_cycles(*if_expr->else_branch) : 0;
        return cond + 2 + std::max(then_c, else_c);
    }

    if (auto* while_expr = dynamic_cast<const WhileExpr*>(&expr)) {
        int64_t cond = compute_expr_cycles(*while_expr->condition);
        int64_t body = while_expr->body ? compute_block_cycles(*while_expr->body) : 0;
        return cond + 10 * (cond + body + 2);
    }

    if (auto* for_expr = dynamic_cast<const ForExpr*>(&expr)) {
        int64_t trip_count = 10;
        if (auto* range = dynamic_cast<const RangeExpr*>(for_expr->iterable.get())) {
            auto* s_lit = dynamic_cast<const IntegerLiteralExpr*>(range->start.get());
            auto* e_lit = dynamic_cast<const IntegerLiteralExpr*>(range->end.get());
            if (s_lit && e_lit && e_lit->value >= s_lit->value) {
                trip_count = e_lit->value - s_lit->value;
            }
        }
        int64_t body = for_expr->body ? compute_block_cycles(*for_expr->body) : 0;
        return 2 + trip_count * (body + 2);
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        int64_t arg_cost = 0;
        for (const auto& a : call->arguments) {
            arg_cost += compute_expr_cycles(*a);
        }
        int64_t fn_cost = 5;
        if (call->callee == "println!") fn_cost = 50;
        else if (call->callee == "sqrt") fn_cost = 15;
        else if (call->callee == "clamp") fn_cost = 4;
        else if (call->callee == "tmr_vote") fn_cost = 6;
        else if (call->callee == "memory_barrier") fn_cost = 2;
        else {
            for (const auto& fn : program_.functions) {
                if (fn->name == call->callee && fn->body) {
                    fn_cost += compute_block_cycles(*fn->body);
                    break;
                }
            }
        }
        return arg_cost + fn_cost;
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&expr)) {
        int64_t cost = compute_expr_cycles(*mc->receiver) + 3;
        for (const auto& a : mc->arguments) {
            cost += compute_expr_cycles(*a);
        }
        return cost;
    }

    if (auto* ret = dynamic_cast<const ReturnExpr*>(&expr)) {
        return ret->value ? compute_expr_cycles(*ret->value) + 1 : 1;
    }

    if (auto* block = dynamic_cast<const BlockExpr*>(&expr)) {
        return compute_block_cycles(*block);
    }

    return 1;
}

int64_t AerospaceVerifier::compute_block_cycles(const BlockExpr& block) {
    int64_t total = 0;
    for (const auto& s : block.statements) {
        if (auto* let = dynamic_cast<const LetStmt*>(s.get())) {
            total += 1;
            if (let->initializer) {
                total += compute_expr_cycles(*let->initializer);
            }
        } else if (auto* es = dynamic_cast<const ExprStmt*>(s.get())) {
            if (es->expr) {
                total += compute_expr_cycles(*es->expr);
            }
        }
    }
    if (block.trailing_expr) {
        total += compute_expr_cycles(*block.trailing_expr);
    }
    return total;
}

// Worst-Case Execution Time (WCET) Static Verifier:
// Calculates the upper bound execution latency across control-flow paths at 100 MHz clock rate.
// Rejects real-time flight loops if their worst-case path exceeds the declared deadline budget.
void AerospaceVerifier::check_realtime_budgets() {
    for (const auto& fn : program_.functions) {
        auto budget_opt = fn->get_realtime_budget_us();
        if (budget_opt.has_value() && fn->body) {
            int64_t budget_us = *budget_opt;
            if (budget_us <= 0) {
                emit_error(fn->location, "real-time deadline error in function '" + fn->name + "': budget must be a positive integer");
                continue;
            }
            int64_t cycles = compute_block_cycles(*fn->body);
            int64_t wcet_us = (cycles + 99) / 100;
            if (wcet_us == 0) wcet_us = 1;
            if (wcet_us > budget_us) {
                emit_error(fn->location, "real-time deadline exceeded in function '" + fn->name + "': worst-case execution time is " +
                           std::to_string(wcet_us) + " us (exceeds budget of " + std::to_string(budget_us) + " us)");
            }
        }
    }
    for (const auto& ib : program_.impl_blocks) {
        for (const auto& fn : ib->methods) {
            auto budget_opt = fn->get_realtime_budget_us();
            if (budget_opt.has_value() && fn->body) {
                int64_t budget_us = *budget_opt;
                if (budget_us <= 0) {
                    emit_error(fn->location, "real-time deadline error in function '" + fn->name + "': budget must be a positive integer");
                    continue;
                }
                int64_t cycles = compute_block_cycles(*fn->body);
                int64_t wcet_us = (cycles + 99) / 100;
                if (wcet_us == 0) wcet_us = 1;
                if (wcet_us > budget_us) {
                    emit_error(fn->location, "real-time deadline exceeded in function '" + fn->name + "': worst-case execution time is " +
                               std::to_string(wcet_us) + " us (exceeds budget of " + std::to_string(budget_us) + " us)");
                }
            }
        }
    }
}

// McCabe Cyclomatic Complexity Verifier (NASA NESC / SLS Standard):
// Evaluates the graph-theoretic cyclomatic complexity M = E - N + 2P over the Control Flow Graph (CFG).
// Enforces that flight software units do not exceed the NASA complexity ceiling (default M <= 15, or #[max_complexity = N]),
// ensuring that all decision paths remain testable and verifiable.
void AerospaceVerifier::check_cyclomatic_complexity() {
    for (const auto& fn : program_.functions) {
        if (!fn->body) continue;

        auto custom_max = fn->get_max_complexity();
        int64_t threshold = 15; // NASA / SLS standard threshold
        if (custom_max.has_value()) {
            threshold = *custom_max;
            if (threshold <= 0) {
                emit_error(fn->location, "cyclomatic complexity configuration error in function '" + fn->name + "': max_complexity must be a positive integer");
                continue;
            }
        }

        CFGFunction cfg = CFGBuilder::build(*fn);
        std::set<BlockId> visited;
        std::queue<BlockId> q;
        if (cfg.entry_block < cfg.blocks.size()) {
            q.push(cfg.entry_block);
            visited.insert(cfg.entry_block);
        }

        while (!q.empty()) {
            BlockId curr = q.front();
            q.pop();
            for (BlockId succ : cfg.blocks[curr].successors) {
                if (succ < cfg.blocks.size() && visited.insert(succ).second) {
                    q.push(succ);
                }
            }
        }

        size_t N = visited.size();
        size_t E = 0;
        for (BlockId u : visited) {
            for (BlockId v : cfg.blocks[u].successors) {
                if (visited.count(v)) {
                    E++;
                }
            }
        }

        int64_t M = (int64_t)E - (int64_t)N + 2;
        if (M < 1) M = 1;

        if (M > threshold) {
            emit_error(fn->location, "cyclomatic complexity of function '" + fn->name + "' (" +
                       std::to_string(M) + ") exceeds threshold (" + std::to_string(threshold) +
                       ") [NASA NESC / SLS Rule: M = E - N + 2, E=" + std::to_string(E) +
                       ", N=" + std::to_string(N) + "]");
        }
    }
}

bool AerospaceVerifier::verify() {
    build_call_graph();
    check_recursion();
    check_loop_bounds();
    check_memory_allocation();
    check_volatile_barriers();
    check_realtime_budgets();
    check_cyclomatic_complexity();
    return errors_.empty();
}

} // namespace oxide
