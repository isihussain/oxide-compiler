#include "aerospace.hpp"
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

bool AerospaceVerifier::verify() {
    build_call_graph();
    check_recursion();
    return errors_.empty();
}

} // namespace oxide
