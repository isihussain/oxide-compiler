#include "typeck.hpp"
#include <optional>
#include <queue>
#include <set>
#include <sstream>

namespace oxide {

static bool extract_const_int(const Expression* expr, int64_t& out_val) {
    if (!expr) return false;
    if (auto* lit = dynamic_cast<const IntegerLiteralExpr*>(expr)) {
        out_val = lit->value;
        return true;
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(expr)) {
        if (un->op == UnaryOp::Negate) {
            int64_t inner = 0;
            if (extract_const_int(un->operand.get(), inner)) {
                out_val = -inner;
                return true;
            }
        }
    }
    return false;
}

TypeChecker::TypeChecker(Program& program) : program_(program) {}

void TypeChecker::emit_error(SourceLocation loc, const std::string& message) {
    std::ostringstream ss;
    ss << "error[" << loc.line << ":" << loc.column << "]: " << message;
    errors_.push_back(ss.str());
}

void TypeChecker::enter_scope() {
    scopes_.emplace_back();
}

void TypeChecker::exit_scope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void TypeChecker::declare_variable(const std::string& name, const Type& type, bool is_mut, SourceLocation loc) {
    if (scopes_.empty()) return;
    auto& current = scopes_.back();
    if (current.find(name) != current.end()) {
        emit_error(loc, "re-declaration of variable '" + name + "' in same scope");
        return;
    }
    current[name] = {type, is_mut};
}

std::optional<std::pair<Type, bool>> TypeChecker::lookup_variable(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

void TypeChecker::register_structs() {
    for (const auto& s : program_.structs) {
        if (struct_table_.find(s->name) != struct_table_.end()) {
            emit_error(s->location, "duplicate struct definition: '" + s->name + "'");
            continue;
        }
        StructSignature sig;
        sig.name = s->name;
        sig.type_params = s->type_params;
        sig.location = s->location;
        for (const auto& f : s->fields) {
            if (sig.fields.find(f.name) != sig.fields.end()) {
                emit_error(f.location, "duplicate field '" + f.name + "' in struct '" + s->name + "'");
            }
            sig.fields[f.name] = f.type;
            sig.field_order.push_back(f.name);
        }
        struct_table_[s->name] = std::move(sig);
    }

    struct_table_["ReadOnly"] = StructSignature{"ReadOnly", {}, {}, {}, SourceLocation{0, 0}};
    struct_table_["WriteOnly"] = StructSignature{"WriteOnly", {}, {}, {}, SourceLocation{0, 0}};
    struct_table_["ReadWrite"] = StructSignature{"ReadWrite", {}, {}, {}, SourceLocation{0, 0}};
    struct_table_["VolatileRegister"] = StructSignature{"VolatileRegister", {"T", "Mode"}, {
        {"address", Type{BaseTypeKind::I32, false, false}},
        {"value", Type{BaseTypeKind::GenericParam, "T", false, false}}
    }, {"address", "value"}, SourceLocation{0, 0}};
    struct_table_["Q16_16"] = StructSignature{"Q16_16", {}, {
        {"raw", Type{BaseTypeKind::I32, false, false}}
    }, {"raw"}, SourceLocation{0, 0}};
    struct_table_["SlewRateLimiter"] = StructSignature{"SlewRateLimiter", {}, {
        {"current_position", Type{BaseTypeKind::I32, false, false}},
        {"max_rate", Type{BaseTypeKind::I32, false, false}},
        {"min_bound", Type{BaseTypeKind::I32, false, false}},
        {"max_bound", Type{BaseTypeKind::I32, false, false}}
    }, {"current_position", "max_rate", "min_bound", "max_bound"}, SourceLocation{0, 0}};
}

void TypeChecker::register_enums() {
    for (const auto& e : program_.enums) {
        if (enum_table_.find(e->name) != enum_table_.end() || struct_table_.find(e->name) != struct_table_.end()) {
            emit_error(e->location, "duplicate type definition: '" + e->name + "'");
            continue;
        }
        EnumSignature sig;
        sig.name = e->name;
        sig.location = e->location;
        for (const auto& v : e->variants) {
            if (sig.variants.find(v.name) != sig.variants.end()) {
                emit_error(v.location, "duplicate variant '" + v.name + "' in enum '" + e->name + "'");
            }
            sig.variants[v.name] = EnumVariantSignature{v.name, v.payload_type, v.tag, v.location};
        }
        enum_table_[e->name] = std::move(sig);
    }
}

void TypeChecker::register_state_machines() {
    for (const auto& sm : program_.state_machines) {
        if (enum_table_.find(sm->name) != enum_table_.end() ||
            struct_table_.find(sm->name) != struct_table_.end() ||
            state_machine_table_.find(sm->name) != state_machine_table_.end()) {
            emit_error(sm->location, "duplicate type definition: '" + sm->name + "'");
            continue;
        }

        std::set<std::string> all_states;
        std::set<std::string> normal_states;
        std::set<std::string> terminal_states;

        for (const auto& s : sm->states) {
            if (all_states.count(s)) {
                emit_error(sm->location, "duplicate state '" + s + "' in state_machine '" + sm->name + "'");
            }
            all_states.insert(s);
            normal_states.insert(s);
        }

        for (const auto& s : sm->terminal_states) {
            if (all_states.count(s)) {
                emit_error(sm->location, "duplicate state '" + s + "' in state_machine '" + sm->name + "'");
            }
            all_states.insert(s);
            terminal_states.insert(s);
        }

        if (terminal_states.empty()) {
            emit_error(sm->location, "state_machine '" + sm->name + "' must declare at least one terminal state");
        }

        std::map<std::string, std::vector<std::string>> adj;
        for (const auto& t : sm->transitions) {
            if (!all_states.count(t.from_state)) {
                emit_error(t.location, "unknown state '" + t.from_state + "' in transition for state_machine '" + sm->name + "'");
                continue;
            }
            if (!all_states.count(t.to_state)) {
                emit_error(t.location, "unknown state '" + t.to_state + "' in transition for state_machine '" + sm->name + "'");
                continue;
            }
            if (terminal_states.count(t.from_state)) {
                emit_error(t.location, "terminal state '" + t.from_state + "' cannot have outgoing transitions in state_machine '" + sm->name + "'");
                continue;
            }
            adj[t.from_state].push_back(t.to_state);
        }

        // Aerospace Mission Flight State Machine Verification:
        // NASA and ESA flight software standards mandate that flight sequences:
        //  1. Never enter a deadlock sink (a non-terminal state lacking any outgoing transitions).
        //  2. Never enter an unescapable trap cycle (a closed strongly connected component that
        //     cannot transition to an emergency abort, safe-hold, or mission completion terminal state).

        // Invariant 1: Non-terminal states must have outgoing transitions.
        for (const auto& s : normal_states) {
            if (adj.find(s) == adj.end() || adj[s].empty()) {
                emit_error(sm->location, "deadlock detected in state_machine '" + sm->name + "': non-terminal state '" + s + "' has no outgoing transitions");
            }
        }

        // Invariant 2: Terminal/Abort Reachability (Breadth-First Search).
        // Traverses the directed state transition graph starting from each non-terminal state.
        // If a state cannot reach ANY terminal state, the flight software could freeze indefinitely.
        for (const auto& s : normal_states) {
            std::set<std::string> visited;
            std::queue<std::string> q;
            q.push(s);
            visited.insert(s);
            bool reaches_terminal = false;
            while (!q.empty()) {
                std::string cur = q.front();
                q.pop();
                if (terminal_states.count(cur)) {
                    reaches_terminal = true;
                    break;
                }
                auto it = adj.find(cur);
                if (it != adj.end()) {
                    for (const auto& nxt : it->second) {
                        if (!visited.count(nxt)) {
                            visited.insert(nxt);
                            q.push(nxt);
                        }
                    }
                }
            }
            if (!reaches_terminal && !terminal_states.empty()) {
                emit_error(sm->location, "deadlock trap detected in state_machine '" + sm->name + "': state '" + s + "' cannot reach any terminal state");
            }
        }

        // Register variants in enum_table_ so match expressions and variant expressions work
        EnumSignature enum_sig;
        enum_sig.name = sm->name;
        enum_sig.location = sm->location;
        int tag = 0;
        for (const auto& s : sm->states) {
            enum_sig.variants[s] = EnumVariantSignature{s, std::nullopt, tag++, sm->location};
        }
        for (const auto& s : sm->terminal_states) {
            enum_sig.variants[s] = EnumVariantSignature{s, std::nullopt, tag++, sm->location};
        }
        enum_table_[sm->name] = std::move(enum_sig);

        // Register transition functions in function_table_
        Type sm_type{BaseTypeKind::Named, sm->name, false, false};
        Type bool_type{BaseTypeKind::Bool, false, false};

        function_table_[sm->name + "::can_transition"] = FunctionSignature{
            sm->name + "::can_transition",
            {},
            {sm_type, sm_type},
            bool_type,
            sm->location
        };

        function_table_[sm->name + "::transition"] = FunctionSignature{
            sm->name + "::transition",
            {},
            {sm_type, sm_type},
            sm_type,
            sm->location
        };

        // Store state machine signature
        StateMachineSignature sm_sig;
        sm_sig.name = sm->name;
        sm_sig.states = sm->states;
        sm_sig.terminal_states = sm->terminal_states;
        sm_sig.transitions = std::move(adj);
        sm_sig.location = sm->location;
        state_machine_table_[sm->name] = std::move(sm_sig);
    }
}

void TypeChecker::register_functions() {
    for (const auto& eb : program_.extern_blocks) {
        for (const auto& decl : eb->declarations) {
            if (function_table_.find(decl.name) != function_table_.end()) {
                emit_error(decl.location, "duplicate function definition: '" + decl.name + "'");
                continue;
            }
            std::vector<Type> params;
            for (const auto& p : decl.parameters) {
                params.push_back(p.type);
            }
            function_table_[decl.name] = FunctionSignature{decl.name, {}, params, decl.return_type, decl.location};
        }
    }

    for (const auto& fn : program_.functions) {
        if (function_table_.find(fn->name) != function_table_.end()) {
            emit_error(fn->location, "duplicate function definition: '" + fn->name + "'");
            continue;
        }
        std::vector<Type> params;
        for (const auto& p : fn->parameters) {
            params.push_back(p.type);
        }
        function_table_[fn->name] = FunctionSignature{fn->name, fn->type_params, params, fn->return_type, fn->location};
    }
}

Type TypeChecker::substitute_type(const Type& type, const std::map<std::string, Type>& subst) {
    Type res = type;
    if (res.kind == BaseTypeKind::GenericParam) {
        auto it = subst.find(res.name);
        if (it != subst.end()) {
            Type replaced = it->second;
            replaced.is_ref = res.is_ref || replaced.is_ref;
            replaced.is_mut_ref = res.is_mut_ref || replaced.is_mut_ref;
            return replaced;
        }
    } else if (res.kind == BaseTypeKind::Named) {
        for (auto& arg : res.type_args) {
            arg = substitute_type(arg, subst);
        }
    }
    return res;
}

// Bidirectional type unification algorithm for generic type parameter inference.
// Solves for type parameters (GenericParam) given concrete observed argument types.
// For example: given formal signature 'fn identity<T>(x: T)' and argument '42' ('i32'),
// unifies pattern 'T' with concrete 'i32' to deduce subst['T'] = i32.
bool TypeChecker::unify_type(const Type& pattern, const Type& concrete, std::map<std::string, Type>& subst) {
    if (pattern.kind == BaseTypeKind::GenericParam) {
        auto it = subst.find(pattern.name);
        if (it != subst.end()) {
            return it->second == concrete;
        }
        subst[pattern.name] = concrete;
        return true;
    }
    if (pattern.kind != concrete.kind) {
        return false;
    }
    if (pattern.kind == BaseTypeKind::Named) {
        if (pattern.name != concrete.name) return false;
        if (pattern.type_args.size() != concrete.type_args.size()) return false;
        for (size_t i = 0; i < pattern.type_args.size(); ++i) {
            if (!unify_type(pattern.type_args[i], concrete.type_args[i], subst)) {
                return false;
            }
        }
        return true;
    }
    return pattern.kind == concrete.kind;
}

void TypeChecker::register_traits() {
    for (const auto& t : program_.traits) {
        if (trait_table_.find(t->name) != trait_table_.end()) {
            emit_error(t->location, "duplicate trait definition: '" + t->name + "'");
            continue;
        }
        TraitSignature sig;
        sig.name = t->name;
        sig.location = t->location;
        for (const auto& m : t->methods) {
            if (sig.methods.find(m.name) != sig.methods.end()) {
                emit_error(m.location, "duplicate method '" + m.name + "' in trait '" + t->name + "'");
            }
            sig.methods[m.name] = TraitMethodSignature{m.name, m.parameters, m.return_type, m.location};
        }
        trait_table_[t->name] = std::move(sig);
    }
}

void TypeChecker::register_impl_blocks() {
    for (const auto& ib : program_.impl_blocks) {
        if (struct_table_.find(ib->target_type) == struct_table_.end()) {
            emit_error(ib->location, "cannot find type '" + ib->target_type + "' in this scope");
        }

        if (ib->trait_name.has_value()) {
            const std::string& trait_name = *ib->trait_name;
            auto t_it = trait_table_.find(trait_name);
            if (t_it == trait_table_.end()) {
                emit_error(ib->location, "cannot find trait '" + trait_name + "' in this scope");
                continue;
            }
            const auto& trait_sig = t_it->second;

            // Check trait completeness: ensure all trait methods are implemented
            for (const auto& [req_name, req_method] : trait_sig.methods) {
                bool found = false;
                for (const auto& m : ib->methods) {
                    if (m->method_name == req_name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_error(ib->location, "missing items in 'impl " + trait_name + " for " +
                               ib->target_type + "': method '" + req_name + "' not implemented");
                }
            }

            // Check signatures of implemented trait methods
            for (const auto& m : ib->methods) {
                auto req_it = trait_sig.methods.find(m->method_name);
                if (req_it == trait_sig.methods.end()) {
                    emit_error(m->location, "method '" + m->method_name + "' is not a member of trait '" + trait_name + "'");
                    continue;
                }
                const auto& req_method = req_it->second;

                if (m->parameters.size() != req_method.parameters.size()) {
                    emit_error(m->location, "method '" + m->method_name + "' has " +
                               std::to_string(m->parameters.size()) + " parameters but the trait requires " +
                               std::to_string(req_method.parameters.size()));
                } else {
                    for (size_t i = 0; i < m->parameters.size(); ++i) {
                        Type exp_type = req_method.parameters[i].type;
                        if (exp_type.name == "Self") {
                            exp_type.name = ib->target_type;
                        }
                        if (m->parameters[i].type != exp_type) {
                            emit_error(m->parameters[i].location, "method '" + m->method_name +
                                       "' has an incompatible type for parameter '" +
                                       m->parameters[i].name + "': expected '" + exp_type.to_string() +
                                       "', found '" + m->parameters[i].type.to_string() + "'");
                        }
                    }
                }

                if (m->return_type != req_method.return_type) {
                    emit_error(m->location, "method '" + m->method_name +
                               "' has an incompatible return type: expected '" +
                               req_method.return_type.to_string() + "', found '" +
                               m->return_type.to_string() + "'");
                }
            }
        }

        // Register methods in method_table_ and function_table_
        for (const auto& m : ib->methods) {
            std::pair<std::string, std::string> key{ib->target_type, m->method_name};
            if (method_table_.find(key) != method_table_.end()) {
                emit_error(m->location, "duplicate method '" + m->method_name + "' for type '" + ib->target_type + "'");
            }
            method_table_[key] = MethodSignature{
                ib->target_type, m->method_name, m->name, m->parameters, m->return_type, ib->trait_name, m->location
            };

            std::vector<Type> param_types;
            for (const auto& p : m->parameters) {
                param_types.push_back(p.type);
            }
            function_table_[m->name] = FunctionSignature{m->name, m->type_params, param_types, m->return_type, m->location};
            function_table_[ib->target_type + "::" + m->method_name] = FunctionSignature{
                ib->target_type + "::" + m->method_name, m->type_params, param_types, m->return_type, m->location
            };
        }
    }
}

bool TypeChecker::check() {
    register_traits();
    register_structs();
    register_enums();
    register_state_machines();
    register_functions();
    register_impl_blocks();
    for (auto& fn : program_.functions) {
        check_function(*fn);
    }
    for (auto& ib : program_.impl_blocks) {
        for (auto& fn : ib->methods) {
            check_function(*fn);
        }
    }
    return errors_.empty();
}

void TypeChecker::check_function(Function& func) {
    auto prev_return = current_function_return_type_;
    current_function_return_type_ = func.return_type;
    enter_scope();
    for (const auto& p : func.parameters) {
        declare_variable(p.name, p.type, p.is_mut, p.location);
    }

    check_block(*func.body, func.return_type, true);
    exit_scope();
    current_function_return_type_ = prev_return;
}

void TypeChecker::check_block(BlockExpr& block, const Type& expected_return_type, bool is_function_root) {
    enter_scope();
    for (auto& stmt : block.statements) {
        check_statement(*stmt);
    }

    Type block_type{BaseTypeKind::Unit, false, false};
    if (block.trailing_expr) {
        block_type = check_expression(*block.trailing_expr);
    } else {
        // If block has no trailing expression, check if any statement is a return matching expected_return_type
        for (const auto& stmt : block.statements) {
            if (auto* es = dynamic_cast<ExprStmt*>(stmt.get())) {
                if (auto* ret = dynamic_cast<ReturnExpr*>(es->expr.get())) {
                    if (ret->evaluated_type == expected_return_type) {
                        block_type = expected_return_type;
                        break;
                    }
                }
            }
        }
    }
    block.evaluated_type = block_type;

    if (is_function_root) {
        if (!(block_type == expected_return_type)) {
            emit_error(block.location, "mismatched types: function expected return type '" +
                       expected_return_type.to_string() + "', but block evaluates to '" +
                       block_type.to_string() + "'");
        }
    }

    exit_scope();
}

void TypeChecker::check_statement(Statement& stmt) {
    if (auto* let = dynamic_cast<LetStmt*>(&stmt)) {
        Type deduced_type = let->declared_type;
        if (let->initializer) {
            Type init_type = check_expression(*let->initializer);
            if (let->declared_type.kind != BaseTypeKind::Unit) {
                if (let->declared_type.name == "VolatileRegister" && init_type.name == "VolatileRegister") {
                    let->initializer->evaluated_type = let->declared_type;
                    init_type = let->declared_type;
                }
                bool is_lit_init = (let->declared_type.kind == BaseTypeKind::PhysicalUnit &&
                                    init_type.kind == BaseTypeKind::I32 &&
                                    dynamic_cast<IntegerLiteralExpr*>(let->initializer.get()) != nullptr);
                bool is_float_init = ((let->declared_type.kind == BaseTypeKind::F32 || let->declared_type.kind == BaseTypeKind::F64) &&
                                      (init_type.kind == BaseTypeKind::F32 || init_type.kind == BaseTypeKind::F64) &&
                                      dynamic_cast<FloatLiteralExpr*>(let->initializer.get()) != nullptr);
                if (is_lit_init || is_float_init) {
                    let->initializer->evaluated_type = let->declared_type;
                } else if (!(let->declared_type == init_type)) {
                    emit_error(let->location, "mismatched types in 'let': expected '" +
                               let->declared_type.to_string() + "', found '" +
                               init_type.to_string() + "'");
                }
            } else {
                deduced_type = init_type;
                let->declared_type = init_type;
            }
        }
        declare_variable(let->var_name, deduced_type, let->is_mutable, let->location);
    } else if (auto* expr_stmt = dynamic_cast<ExprStmt*>(&stmt)) {
        if (expr_stmt->expr) {
            check_expression(*expr_stmt->expr);
        }
    }
}

Type TypeChecker::check_expression(Expression& expr) {
    if (auto* int_lit = dynamic_cast<IntegerLiteralExpr*>(&expr)) {
        int_lit->evaluated_type = Type{BaseTypeKind::I32, false, false};
        return int_lit->evaluated_type;
    }

    if (auto* fl_lit = dynamic_cast<FloatLiteralExpr*>(&expr)) {
        fl_lit->evaluated_type = Type{BaseTypeKind::F64, false, false};
        return fl_lit->evaluated_type;
    }

    if (auto* bool_lit = dynamic_cast<BoolLiteralExpr*>(&expr)) {
        bool_lit->evaluated_type = Type{BaseTypeKind::Bool, false, false};
        return bool_lit->evaluated_type;
    }

    if (auto* str_lit = dynamic_cast<StringLiteralExpr*>(&expr)) {
        str_lit->evaluated_type = Type{BaseTypeKind::Str, "str", true, false};
        return str_lit->evaluated_type;
    }

    if (auto* arr = dynamic_cast<ArrayLiteralExpr*>(&expr)) {
        if (arr->elements.empty()) {
            arr->evaluated_type = Type{BaseTypeKind::Array, "", false, false, {Type{BaseTypeKind::Unit, false, false}}, 0};
            return arr->evaluated_type;
        }
        Type elem_t = check_expression(*arr->elements[0]);
        for (size_t i = 1; i < arr->elements.size(); ++i) {
            Type t = check_expression(*arr->elements[i]);
            if (!(t == elem_t)) {
                emit_error(arr->elements[i]->location, "mismatched types in array element: expected '" +
                           elem_t.to_string() + "', found '" + t.to_string() + "'");
            }
        }
        arr->evaluated_type = Type{BaseTypeKind::Array, "", false, false, {elem_t}, arr->elements.size()};
        return arr->evaluated_type;
    }

    if (auto* rep = dynamic_cast<ArrayRepeatExpr*>(&expr)) {
        Type elem_t = check_expression(*rep->value);
        rep->evaluated_type = Type{BaseTypeKind::Array, "", false, false, {elem_t}, rep->count};
        return rep->evaluated_type;
    }

    if (auto* idx = dynamic_cast<IndexExpr*>(&expr)) {
        Type target_t = check_expression(*idx->target);
        Type idx_t = check_expression(*idx->index);
        if (idx_t.kind != BaseTypeKind::I32) {
            emit_error(idx->index->location, "mismatched types: array index must be 'i32', found '" + idx_t.to_string() + "'");
        }
        if (target_t.kind != BaseTypeKind::Array && target_t.kind != BaseTypeKind::Slice) {
            emit_error(idx->target->location, "cannot index into type '" + target_t.to_string() + "'");
            idx->evaluated_type = Type{BaseTypeKind::I32, false, false};
            return idx->evaluated_type;
        }
        if (auto* int_lit = dynamic_cast<IntegerLiteralExpr*>(idx->index.get())) {
            if (target_t.kind == BaseTypeKind::Array && target_t.array_size > 0) {
                if (int_lit->value < 0 || (size_t)int_lit->value >= target_t.array_size) {
                    emit_error(idx->location, "index out of bounds: the length is " +
                               std::to_string(target_t.array_size) + " but the index is " +
                               std::to_string(int_lit->value));
                }
            }
        }
        Type elem_t = target_t.type_args.empty() ? Type{BaseTypeKind::I32, false, false} : target_t.type_args[0];
        idx->evaluated_type = elem_t;
        return idx->evaluated_type;
    }

    if (auto* range = dynamic_cast<RangeExpr*>(&expr)) {
        Type start_t = check_expression(*range->start);
        Type end_t = check_expression(*range->end);
        if (start_t.kind != BaseTypeKind::I32 || end_t.kind != BaseTypeKind::I32) {
            emit_error(range->location, "range bounds must be 'i32'");
        }
        range->evaluated_type = Type{BaseTypeKind::Named, "Range", false, false, {start_t}};
        return range->evaluated_type;
    }

    if (auto* for_expr = dynamic_cast<ForExpr*>(&expr)) {
        Type iter_t = check_expression(*for_expr->iterable);
        Type elem_t{BaseTypeKind::I32, false, false};
        if (iter_t.kind == BaseTypeKind::Named && iter_t.name == "Range") {
            elem_t = iter_t.type_args.empty() ? Type{BaseTypeKind::I32, false, false} : iter_t.type_args[0];
        } else if (iter_t.kind == BaseTypeKind::Array || iter_t.kind == BaseTypeKind::Slice) {
            elem_t = iter_t.type_args.empty() ? Type{BaseTypeKind::I32, false, false} : iter_t.type_args[0];
        }
        enter_scope();
        declare_variable(for_expr->var_name, elem_t, false, for_expr->location);
        check_block(*for_expr->body, Type{BaseTypeKind::Unit, "", false, false}, false);
        exit_scope();
        for_expr->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
        return for_expr->evaluated_type;
    }

    if (auto* var_expr = dynamic_cast<VariableExpr*>(&expr)) {
        auto var_info = lookup_variable(var_expr->name);
        if (!var_info) {
            emit_error(var_expr->location, "cannot find value '" + var_expr->name + "' in this scope");
            var_expr->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return var_expr->evaluated_type;
        }
        var_expr->evaluated_type = var_info->first;
        return var_expr->evaluated_type;
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        Type left_t = check_expression(*bin->left);
        Type right_t = check_expression(*bin->right);

        if ((left_t.kind == BaseTypeKind::F32 || left_t.kind == BaseTypeKind::F64) &&
            (right_t.kind == BaseTypeKind::F32 || right_t.kind == BaseTypeKind::F64)) {
            if (dynamic_cast<FloatLiteralExpr*>(bin->right.get())) {
                bin->right->evaluated_type = left_t;
                right_t = left_t;
            } else if (dynamic_cast<FloatLiteralExpr*>(bin->left.get())) {
                bin->left->evaluated_type = right_t;
                left_t = right_t;
            }
        }

        switch (bin->op) {
            case BinaryOp::Add:
            case BinaryOp::Sub: {
                // Fixed-Point Arithmetic (Q16.16):
                // Enforces type parity; addition and subtraction operate directly on the raw 32-bit values.
                if (left_t.kind == BaseTypeKind::Named && left_t.name == "Q16_16" &&
                    right_t.kind == BaseTypeKind::Named && right_t.name == "Q16_16") {
                    bin->evaluated_type = left_t;
                    return bin->evaluated_type;
                }
                if (left_t.name == "Q16_16" || right_t.name == "Q16_16") {
                    emit_error(bin->location, "mismatched types in fixed-point arithmetic: cannot operate on '" +
                               left_t.to_string() + "' and '" + right_t.to_string() + "'");
                    bin->evaluated_type = left_t.name == "Q16_16" ? left_t : right_t;
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::I32 && right_t.kind == BaseTypeKind::I32) {
                    bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F32 && right_t.kind == BaseTypeKind::F32) {
                    bin->evaluated_type = Type{BaseTypeKind::F32, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F64 && right_t.kind == BaseTypeKind::F64) {
                    bin->evaluated_type = Type{BaseTypeKind::F64, false, false};
                    return bin->evaluated_type;
                }
                // Mars Climate Orbiter Safeguard (Physical Dimension Addition/Subtraction):
                // Adding or subtracting physical quantities requires exact dimensional equivalence:
                //   [M^a L^b T^c] +/- [M^d L^e T^f] is valid iff (a==d && b==e && c==f).
                // Adding a dimensionless scalar to a dimensional quantity is rejected.
                if (left_t.kind == BaseTypeKind::PhysicalUnit && right_t.kind == BaseTypeKind::PhysicalUnit) {
                    if (!(left_t.dimension == right_t.dimension)) {
                        emit_error(bin->location, "cannot " + std::string(bin->op == BinaryOp::Add ? "add" : "subtract") +
                                   " differing physical dimensions '" + left_t.to_string() + "' and '" + right_t.to_string() + "'");
                    }
                    bin->evaluated_type = left_t;
                    return bin->evaluated_type;
                }
                if ((left_t.kind == BaseTypeKind::PhysicalUnit && right_t.kind == BaseTypeKind::I32) ||
                    (left_t.kind == BaseTypeKind::I32 && right_t.kind == BaseTypeKind::PhysicalUnit)) {
                    emit_error(bin->location, "cannot " + std::string(bin->op == BinaryOp::Add ? "add" : "subtract") +
                               " dimensionless scalar 'i32' to physical dimension '" +
                               (left_t.kind == BaseTypeKind::PhysicalUnit ? left_t.to_string() : right_t.to_string()) + "'");
                    bin->evaluated_type = left_t.kind == BaseTypeKind::PhysicalUnit ? left_t : right_t;
                    return bin->evaluated_type;
                }
                emit_error(bin->location, "mismatched types in arithmetic: cannot operate on '" +
                           left_t.to_string() + "' and '" + right_t.to_string() + "'");
                bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return bin->evaluated_type;
            }
            case BinaryOp::Mul: {
                if (left_t.kind == BaseTypeKind::Named && left_t.name == "Q16_16" &&
                    right_t.kind == BaseTypeKind::Named && right_t.name == "Q16_16") {
                    bin->evaluated_type = left_t;
                    return bin->evaluated_type;
                }
                if (left_t.name == "Q16_16" || right_t.name == "Q16_16") {
                    emit_error(bin->location, "mismatched types in fixed-point arithmetic: cannot operate on '" +
                               left_t.to_string() + "' and '" + right_t.to_string() + "'");
                    bin->evaluated_type = left_t.name == "Q16_16" ? left_t : right_t;
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::I32 && right_t.kind == BaseTypeKind::I32) {
                    bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F32 && right_t.kind == BaseTypeKind::F32) {
                    bin->evaluated_type = Type{BaseTypeKind::F32, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F64 && right_t.kind == BaseTypeKind::F64) {
                    bin->evaluated_type = Type{BaseTypeKind::F64, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::I32 && right_t.kind == BaseTypeKind::PhysicalUnit) {
                    bin->evaluated_type = right_t;
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::PhysicalUnit && right_t.kind == BaseTypeKind::I32) {
                    bin->evaluated_type = left_t;
                    return bin->evaluated_type;
                }
                // Physical Dimension Multiplication:
                // Exponents add algebraically: [M^(m1+m2) L^(l1+l2) T^(t1+t2)].
                // If the sum yields [M^0 L^0 T^0], it cancels out into a dimensionless scalar ('i32').
                if (left_t.kind == BaseTypeKind::PhysicalUnit && right_t.kind == BaseTypeKind::PhysicalUnit) {
                    PhysicalDimension dim{
                        (int8_t)(left_t.dimension.m + right_t.dimension.m),
                        (int8_t)(left_t.dimension.l + right_t.dimension.l),
                        (int8_t)(left_t.dimension.t + right_t.dimension.t)
                    };
                    if (dim.is_dimensionless()) {
                        bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                    } else {
                        bin->evaluated_type = Type{BaseTypeKind::PhysicalUnit, dim, dim.to_unit_name()};
                    }
                    return bin->evaluated_type;
                }
                emit_error(bin->location, "mismatched types in arithmetic: cannot operate on '" +
                           left_t.to_string() + "' and '" + right_t.to_string() + "'");
                bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return bin->evaluated_type;
            }
            case BinaryOp::Div: {
                if (left_t.kind == BaseTypeKind::Named && left_t.name == "Q16_16" &&
                    right_t.kind == BaseTypeKind::Named && right_t.name == "Q16_16") {
                    if (auto* call_r = dynamic_cast<CallExpr*>(bin->right.get())) {
                        if (call_r->callee == "Q16_16::from_int" || call_r->callee == "Q16_16::from_raw" || call_r->callee == "Q16_16::new") {
                            if (!call_r->arguments.empty()) {
                                if (auto* int_lit = dynamic_cast<IntegerLiteralExpr*>(call_r->arguments[0].get())) {
                                    if (int_lit->value == 0) {
                                        emit_error(bin->location, "division by zero: fixed-point divisor is 0");
                                    }
                                }
                            }
                        }
                    }
                    bin->evaluated_type = left_t;
                    return bin->evaluated_type;
                }
                if (left_t.name == "Q16_16" || right_t.name == "Q16_16") {
                    emit_error(bin->location, "mismatched types in fixed-point arithmetic: cannot operate on '" +
                               left_t.to_string() + "' and '" + right_t.to_string() + "'");
                    bin->evaluated_type = left_t.name == "Q16_16" ? left_t : right_t;
                    return bin->evaluated_type;
                }
                // Static check for division by zero literal
                if (auto* r_fl = dynamic_cast<FloatLiteralExpr*>(bin->right.get())) {
                    if (r_fl->value == 0.0) {
                        emit_error(bin->location, "division by zero: floating-point divisor is 0.0");
                    }
                } else if (auto* un_r = dynamic_cast<UnaryExpr*>(bin->right.get())) {
                    if (un_r->op == UnaryOp::Negate) {
                        if (auto* inner_fl = dynamic_cast<FloatLiteralExpr*>(un_r->operand.get())) {
                            if (inner_fl->value == 0.0) {
                                emit_error(bin->location, "division by zero: floating-point divisor is 0.0");
                            }
                        } else if (auto* inner_int = dynamic_cast<IntegerLiteralExpr*>(un_r->operand.get())) {
                            if (inner_int->value == 0) {
                                emit_error(bin->location, "division by zero: integer divisor is 0");
                            }
                        }
                    }
                } else if (auto* r_int = dynamic_cast<IntegerLiteralExpr*>(bin->right.get())) {
                    if (r_int->value == 0) {
                        emit_error(bin->location, "division by zero: integer divisor is 0");
                    }
                }

                if (left_t.kind == BaseTypeKind::I32 && right_t.kind == BaseTypeKind::I32) {
                    bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F32 && right_t.kind == BaseTypeKind::F32) {
                    bin->evaluated_type = Type{BaseTypeKind::F32, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F64 && right_t.kind == BaseTypeKind::F64) {
                    bin->evaluated_type = Type{BaseTypeKind::F64, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::PhysicalUnit && right_t.kind == BaseTypeKind::I32) {
                    bin->evaluated_type = left_t;
                    return bin->evaluated_type;
                }
                // Physical Dimension Division:
                // Exponents subtract algebraically: [M^(m1-m2) L^(l1-l2) T^(t1-t2)].
                // If the exponents reach [M^0 L^0 T^0], it cancels out into a dimensionless scalar ('i32').
                if (left_t.kind == BaseTypeKind::PhysicalUnit && right_t.kind == BaseTypeKind::PhysicalUnit) {
                    PhysicalDimension dim{
                        (int8_t)(left_t.dimension.m - right_t.dimension.m),
                        (int8_t)(left_t.dimension.l - right_t.dimension.l),
                        (int8_t)(left_t.dimension.t - right_t.dimension.t)
                    };
                    if (dim.is_dimensionless()) {
                        bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                    } else {
                        bin->evaluated_type = Type{BaseTypeKind::PhysicalUnit, dim, dim.to_unit_name()};
                    }
                    return bin->evaluated_type;
                }
                emit_error(bin->location, "mismatched types in arithmetic: cannot operate on '" +
                           left_t.to_string() + "' and '" + right_t.to_string() + "'");
                bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return bin->evaluated_type;
            }
            case BinaryOp::Equal:
            case BinaryOp::NotEqual: {
                if (!(left_t == right_t)) {
                    emit_error(bin->location, "cannot compare differing types '" +
                               left_t.to_string() + "' and '" + right_t.to_string() + "'");
                }
                bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return bin->evaluated_type;
            }
            case BinaryOp::Less:
            case BinaryOp::LessEqual:
            case BinaryOp::Greater:
            case BinaryOp::GreaterEqual: {
                if (left_t.kind == BaseTypeKind::Named && left_t.name == "Q16_16" &&
                    right_t.kind == BaseTypeKind::Named && right_t.name == "Q16_16") {
                    bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::I32 && right_t.kind == BaseTypeKind::I32) {
                    bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F32 && right_t.kind == BaseTypeKind::F32) {
                    bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                    return bin->evaluated_type;
                }
                if (left_t.kind == BaseTypeKind::F64 && right_t.kind == BaseTypeKind::F64) {
                    bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                    return bin->evaluated_type;
                }
                // Physical Dimension Relational Comparison:
                // Quantities can only be compared if their physical dimensions match identically.
                if (left_t.kind == BaseTypeKind::PhysicalUnit && right_t.kind == BaseTypeKind::PhysicalUnit) {
                    if (!(left_t.dimension == right_t.dimension)) {
                        emit_error(bin->location, "cannot compare differing physical dimensions '" +
                                   left_t.to_string() + "' and '" + right_t.to_string() + "'");
                    }
                    bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                    return bin->evaluated_type;
                }
                emit_error(bin->location, "order comparison only supported for identical arithmetic types");
                bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return bin->evaluated_type;
            }
        }
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        Type inner_t = check_expression(*un->operand);
        switch (un->op) {
            case UnaryOp::Negate: {
                if (inner_t.kind == BaseTypeKind::Named && inner_t.name == "Q16_16") {
                    un->evaluated_type = inner_t;
                    return un->evaluated_type;
                }
                if (inner_t.kind != BaseTypeKind::I32 && inner_t.kind != BaseTypeKind::PhysicalUnit &&
                    inner_t.kind != BaseTypeKind::F32 && inner_t.kind != BaseTypeKind::F64) {
                    emit_error(un->location, "unary minus only valid on numbers or physical units");
                }
                un->evaluated_type = inner_t;
                return un->evaluated_type;
            }
            case UnaryOp::Not: {
                if (inner_t.kind != BaseTypeKind::Bool) {
                    emit_error(un->location, "unary not '!' only valid on 'bool'");
                }
                un->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return un->evaluated_type;
            }
            case UnaryOp::Ref: {
                Type ref_t = inner_t;
                ref_t.is_ref = true;
                ref_t.is_mut_ref = false;
                un->evaluated_type = ref_t;
                return un->evaluated_type;
            }
            case UnaryOp::RefMut: {
                Type ref_t = inner_t;
                ref_t.is_ref = true;
                ref_t.is_mut_ref = true;
                un->evaluated_type = ref_t;
                return un->evaluated_type;
            }
            case UnaryOp::Deref: {
                if (!inner_t.is_ref) {
                    emit_error(un->location, "type '" + inner_t.to_string() + "' cannot be dereferenced");
                }
                Type deref_t = inner_t;
                deref_t.is_ref = false;
                deref_t.is_mut_ref = false;
                un->evaluated_type = deref_t;
                return un->evaluated_type;
            }
        }
    }

    if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        Type target_t = check_expression(*assign->target);
        Type val_t = check_expression(*assign->value);
        bool is_lit_assign = (target_t.kind == BaseTypeKind::PhysicalUnit &&
                              val_t.kind == BaseTypeKind::I32 &&
                              dynamic_cast<IntegerLiteralExpr*>(assign->value.get()) != nullptr);
        bool is_float_assign = ((target_t.kind == BaseTypeKind::F32 || target_t.kind == BaseTypeKind::F64) &&
                                (val_t.kind == BaseTypeKind::F32 || val_t.kind == BaseTypeKind::F64) &&
                                dynamic_cast<FloatLiteralExpr*>(assign->value.get()) != nullptr);
        if (is_lit_assign || is_float_assign) {
            assign->value->evaluated_type = target_t;
        } else if (!(target_t == val_t)) {
            emit_error(assign->location, "mismatched types in assignment: target is '" +
                       target_t.to_string() + "', value is '" + val_t.to_string() + "'");
        }
        assign->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
        return assign->evaluated_type;
    }

    if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        if (call->callee == "println!") {
            for (auto& a : call->arguments) {
                check_expression(*a);
            }
            call->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return call->evaluated_type;
        }
        if (call->callee == "assert!") {
            if (call->arguments.empty() || call->arguments.size() > 2) {
                emit_error(call->location, "assert! takes 1 or 2 arguments");
            } else {
                Type cond_t = check_expression(*call->arguments[0]);
                if (cond_t.kind != BaseTypeKind::Bool) {
                    emit_error(call->arguments[0]->location, "mismatched types: expected 'bool', found '" + cond_t.to_string() + "'");
                }
                if (call->arguments.size() > 1) {
                    check_expression(*call->arguments[1]);
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return call->evaluated_type;
        }
        if (call->callee == "panic!") {
            for (auto& a : call->arguments) {
                check_expression(*a);
            }
            call->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return call->evaluated_type;
        }
        if (call->callee == "sqrt") {
            if (call->arguments.size() != 1) {
                emit_error(call->location, "sqrt takes exactly 1 argument");
                call->evaluated_type = Type{BaseTypeKind::F32, false, false};
                return call->evaluated_type;
            }
            Type arg_t = check_expression(*call->arguments[0]);
            if (arg_t.kind != BaseTypeKind::F32 && arg_t.kind != BaseTypeKind::F64) {
                emit_error(call->arguments[0]->location, "mismatched types: sqrt requires 'f32' or 'f64', found '" + arg_t.to_string() + "'");
            }
            if (auto* fl_lit = dynamic_cast<FloatLiteralExpr*>(call->arguments[0].get())) {
                if (fl_lit->value < 0.0) {
                    emit_error(call->arguments[0]->location, "domain error: square root of negative float literal (" + std::to_string(fl_lit->value) + ")");
                }
            } else if (auto* un = dynamic_cast<UnaryExpr*>(call->arguments[0].get())) {
                if (un->op == UnaryOp::Negate) {
                    if (auto* inner_fl = dynamic_cast<FloatLiteralExpr*>(un->operand.get())) {
                        if (inner_fl->value > 0.0) {
                            emit_error(call->arguments[0]->location, "domain error: square root of negative float literal (-" + std::to_string(inner_fl->value) + ")");
                        }
                    }
                }
            }
            call->evaluated_type = arg_t;
            return call->evaluated_type;
        }
        if (call->callee == "clamp") {
            if (call->arguments.size() != 3) {
                emit_error(call->location, "clamp takes exactly 3 arguments (value, min, max)");
                call->evaluated_type = Type{BaseTypeKind::F32, false, false};
                return call->evaluated_type;
            }
            Type val_t = check_expression(*call->arguments[0]);
            Type min_t = check_expression(*call->arguments[1]);
            Type max_t = check_expression(*call->arguments[2]);
            if (val_t.kind == BaseTypeKind::F32 || val_t.kind == BaseTypeKind::F64) {
                if (dynamic_cast<FloatLiteralExpr*>(call->arguments[1].get())) {
                    call->arguments[1]->evaluated_type = val_t;
                    min_t = val_t;
                }
                if (dynamic_cast<FloatLiteralExpr*>(call->arguments[2].get())) {
                    call->arguments[2]->evaluated_type = val_t;
                    max_t = val_t;
                }
            }
            if (!(val_t == min_t) || !(val_t == max_t)) {
                emit_error(call->location, "mismatched types in clamp arguments");
            }
            auto* min_fl = dynamic_cast<FloatLiteralExpr*>(call->arguments[1].get());
            auto* max_fl = dynamic_cast<FloatLiteralExpr*>(call->arguments[2].get());
            if (min_fl && max_fl && min_fl->value > max_fl->value) {
                emit_error(call->location, "clamp bounds error: min value is greater than max value");
            }
            auto* min_int = dynamic_cast<IntegerLiteralExpr*>(call->arguments[1].get());
            auto* max_int = dynamic_cast<IntegerLiteralExpr*>(call->arguments[2].get());
            if (min_int && max_int && min_int->value > max_int->value) {
                emit_error(call->location, "clamp bounds error: min value is greater than max value");
            }
            call->evaluated_type = val_t;
            return call->evaluated_type;
        }
        if (call->callee == "tmr_vote") {
            if (call->arguments.size() != 3) {
                emit_error(call->location, "tmr_vote takes exactly 3 arguments (channel0, channel1, channel2)");
                call->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return call->evaluated_type;
            }
            Type t0 = check_expression(*call->arguments[0]);
            Type t1 = check_expression(*call->arguments[1]);
            Type t2 = check_expression(*call->arguments[2]);

            if (t0.kind == BaseTypeKind::F32 || t0.kind == BaseTypeKind::F64) {
                if (dynamic_cast<FloatLiteralExpr*>(call->arguments[1].get())) {
                    call->arguments[1]->evaluated_type = t0;
                    t1 = t0;
                }
                if (dynamic_cast<FloatLiteralExpr*>(call->arguments[2].get())) {
                    call->arguments[2]->evaluated_type = t0;
                    t2 = t0;
                }
            }

            if (!(t0 == t1) || !(t0 == t2)) {
                emit_error(call->location, "mismatched types across TMR channels: all 3 channels must have identical types");
            }

            auto* i0 = dynamic_cast<IntegerLiteralExpr*>(call->arguments[0].get());
            auto* i1 = dynamic_cast<IntegerLiteralExpr*>(call->arguments[1].get());
            auto* i2 = dynamic_cast<IntegerLiteralExpr*>(call->arguments[2].get());
            if (i0 && i1 && i2) {
                if (i0->value != i1->value && i0->value != i2->value && i1->value != i2->value) {
                    emit_error(call->location, "radiation mitigation error: static TMR voter discordance (no majority between constants " +
                               std::to_string(i0->value) + ", " + std::to_string(i1->value) + ", " + std::to_string(i2->value) + ")");
                }
            }
            auto* f0 = dynamic_cast<FloatLiteralExpr*>(call->arguments[0].get());
            auto* f1 = dynamic_cast<FloatLiteralExpr*>(call->arguments[1].get());
            auto* f2 = dynamic_cast<FloatLiteralExpr*>(call->arguments[2].get());
            if (f0 && f1 && f2) {
                if (f0->value != f1->value && f0->value != f2->value && f1->value != f2->value) {
                    emit_error(call->location, "radiation mitigation error: static TMR voter discordance (no majority between constants)");
                }
            }

            call->evaluated_type = t0;
            return call->evaluated_type;
        }

        if (call->callee == "VolatileRegister::new") {
            if (call->arguments.empty() || call->arguments.size() > 2) {
                emit_error(call->location, "VolatileRegister::new takes 1 or 2 arguments (address, [initial_value])");
            } else {
                Type addr_t = check_expression(*call->arguments[0]);
                if (addr_t.kind != BaseTypeKind::I32) {
                    emit_error(call->arguments[0]->location, "VolatileRegister address must be 'i32'");
                }
                if (call->arguments.size() == 2) {
                    check_expression(*call->arguments[1]);
                }
            }
            Type t{BaseTypeKind::Named, "VolatileRegister", false, false};
            if (!call->type_args.empty()) {
                t.type_args = call->type_args;
            } else {
                t.type_args = {Type{BaseTypeKind::I32, false, false}, Type{BaseTypeKind::Named, "ReadWrite", false, false}};
            }
            call->evaluated_type = t;
            return call->evaluated_type;
        }

        if (call->callee == "memory_barrier") {
            if (!call->arguments.empty()) {
                emit_error(call->location, "memory_barrier takes 0 arguments");
            }
            call->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return call->evaluated_type;
        }

        if (call->callee == "Q16_16::from_int" || call->callee == "Q16_16::new") {
            if (call->arguments.size() != 1) {
                emit_error(call->location, "Q16_16::from_int takes exactly 1 argument (i32)");
            } else {
                Type arg_t = check_expression(*call->arguments[0]);
                if (arg_t.kind != BaseTypeKind::I32) {
                    emit_error(call->arguments[0]->location, "mismatched types in Q16_16::from_int: expected 'i32', found '" + arg_t.to_string() + "'");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "Q16_16", false, false};
            return call->evaluated_type;
        }
        if (call->callee == "Q16_16::from_raw") {
            if (call->arguments.size() != 1) {
                emit_error(call->location, "Q16_16::from_raw takes exactly 1 argument (i32)");
            } else {
                Type arg_t = check_expression(*call->arguments[0]);
                if (arg_t.kind != BaseTypeKind::I32) {
                    emit_error(call->arguments[0]->location, "mismatched types in Q16_16::from_raw: expected 'i32', found '" + arg_t.to_string() + "'");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "Q16_16", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "SlewRateLimiter::new") {
            if (call->arguments.size() != 4) {
                emit_error(call->location, "SlewRateLimiter::new takes exactly 4 arguments (initial_pos, max_rate, min_bound, max_bound)");
            } else {
                for (size_t i = 0; i < 4; ++i) {
                    Type t = check_expression(*call->arguments[i]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(call->arguments[i]->location, "SlewRateLimiter parameter must be 'i32'");
                    }
                }
                int64_t init_val = 0, rate_val = 0, min_val = 0, max_val = 0;
                bool has_init = extract_const_int(call->arguments[0].get(), init_val);
                bool has_rate = extract_const_int(call->arguments[1].get(), rate_val);
                bool has_min = extract_const_int(call->arguments[2].get(), min_val);
                bool has_max = extract_const_int(call->arguments[3].get(), max_val);

                if (has_min && has_max && min_val >= max_val) {
                    emit_error(call->location, "invalid actuator configuration: min_bound (" +
                               std::to_string(min_val) + ") must be strictly less than max_bound (" +
                               std::to_string(max_val) + ")");
                }
                if (has_rate && rate_val <= 0) {
                    emit_error(call->location, "invalid actuator configuration: max_rate (" +
                               std::to_string(rate_val) + ") must be positive");
                }
                if (has_init && has_min && has_max) {
                    if (init_val < min_val || init_val > max_val) {
                        emit_error(call->location, "invalid actuator configuration: initial_pos (" +
                                   std::to_string(init_val) + ") exceeds physical bounds [" +
                                   std::to_string(min_val) + ", " + std::to_string(max_val) + "]");
                    }
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "SlewRateLimiter", false, false};
            return call->evaluated_type;
        }

        auto fn_it = function_table_.find(call->callee);
        if (fn_it == function_table_.end()) {
            emit_error(call->location, "cannot find function '" + call->callee + "' in this scope");
            call->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return call->evaluated_type;
        }

        const auto& sig = fn_it->second;
        if (call->arguments.size() != sig.param_types.size()) {
            emit_error(call->location, "function '" + call->callee + "' takes " +
                       std::to_string(sig.param_types.size()) + " arguments but " +
                       std::to_string(call->arguments.size()) + " were supplied");
            call->evaluated_type = sig.return_type;
            return call->evaluated_type;
        }

        std::map<std::string, Type> subst;
        if (!sig.type_params.empty()) {
            if (!call->type_args.empty()) {
                if (call->type_args.size() != sig.type_params.size()) {
                    emit_error(call->location, "function '" + call->callee + "' takes " +
                               std::to_string(sig.type_params.size()) + " type arguments but " +
                               std::to_string(call->type_args.size()) + " were supplied");
                } else {
                    for (size_t i = 0; i < sig.type_params.size(); ++i) {
                        subst[sig.type_params[i]] = call->type_args[i];
                    }
                }
            }

            // Type check arguments and infer missing type parameters
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                Type arg_t = check_expression(*call->arguments[i]);
                unify_type(sig.param_types[i], arg_t, subst);
            }

            // Fill in call->type_args if it was inferred
            if (call->type_args.empty()) {
                for (const auto& tp : sig.type_params) {
                    auto it = subst.find(tp);
                    if (it != subst.end()) {
                        call->type_args.push_back(it->second);
                    } else {
                        emit_error(call->location, "cannot infer type argument for type parameter '" + tp +
                                   "' in call to '" + call->callee + "'");
                    }
                }
            }

            // Check argument types against substituted parameter types
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                Type expected_t = substitute_type(sig.param_types[i], subst);
                Type arg_t = call->arguments[i]->evaluated_type;
                bool is_lit = (expected_t.kind == BaseTypeKind::PhysicalUnit &&
                               arg_t.kind == BaseTypeKind::I32 &&
                               dynamic_cast<IntegerLiteralExpr*>(call->arguments[i].get()) != nullptr);
                bool is_float_lit = ((expected_t.kind == BaseTypeKind::F32 || expected_t.kind == BaseTypeKind::F64) &&
                                     (arg_t.kind == BaseTypeKind::F32 || arg_t.kind == BaseTypeKind::F64) &&
                                     dynamic_cast<FloatLiteralExpr*>(call->arguments[i].get()) != nullptr);
                if (is_lit || is_float_lit) {
                    call->arguments[i]->evaluated_type = expected_t;
                } else if (!(arg_t == expected_t)) {
                    emit_error(call->arguments[i]->location, "mismatched type in argument " +
                               std::to_string(i + 1) + ": expected '" + expected_t.to_string() +
                               "', found '" + arg_t.to_string() + "'");
                }
            }
            call->evaluated_type = substitute_type(sig.return_type, subst);
            return call->evaluated_type;
        } else {
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                Type arg_t = check_expression(*call->arguments[i]);
                bool is_lit = (sig.param_types[i].kind == BaseTypeKind::PhysicalUnit &&
                               arg_t.kind == BaseTypeKind::I32 &&
                               dynamic_cast<IntegerLiteralExpr*>(call->arguments[i].get()) != nullptr);
                bool is_float_lit = ((sig.param_types[i].kind == BaseTypeKind::F32 || sig.param_types[i].kind == BaseTypeKind::F64) &&
                                     (arg_t.kind == BaseTypeKind::F32 || arg_t.kind == BaseTypeKind::F64) &&
                                     dynamic_cast<FloatLiteralExpr*>(call->arguments[i].get()) != nullptr);
                if (is_lit || is_float_lit) {
                    call->arguments[i]->evaluated_type = sig.param_types[i];
                } else if (!(arg_t == sig.param_types[i])) {
                    emit_error(call->arguments[i]->location, "mismatched type in argument " +
                               std::to_string(i + 1) + ": expected '" + sig.param_types[i].to_string() +
                               "', found '" + arg_t.to_string() + "'");
                }
            }
            call->evaluated_type = sig.return_type;
            return call->evaluated_type;
        }
    }

    if (auto* if_expr = dynamic_cast<IfExpr*>(&expr)) {
        Type cond_t = check_expression(*if_expr->condition);
        if (cond_t.kind != BaseTypeKind::Bool) {
            emit_error(if_expr->condition->location, "mismatched types: expected 'bool', found '" +
                       cond_t.to_string() + "' in 'if' condition");
        }

        check_block(*if_expr->then_branch, Type{BaseTypeKind::Unit, false, false}, false);
        Type then_t = if_expr->then_branch->evaluated_type;

        if (if_expr->else_branch) {
            check_block(*if_expr->else_branch, Type{BaseTypeKind::Unit, false, false}, false);
            Type else_t = if_expr->else_branch->evaluated_type;
            if (!(then_t == else_t)) {
                emit_error(if_expr->location, "'if' and 'else' have incompatible types: '" +
                           then_t.to_string() + "' versus '" + else_t.to_string() + "'");
            }
            if_expr->evaluated_type = then_t;
        } else {
            if_expr->evaluated_type = Type{BaseTypeKind::Unit, false, false};
        }
        return if_expr->evaluated_type;
    }

    if (auto* ret = dynamic_cast<ReturnExpr*>(&expr)) {
        if (ret->value) {
            ret->evaluated_type = check_expression(*ret->value);
        } else {
            ret->evaluated_type = Type{BaseTypeKind::Unit, false, false};
        }
        if (current_function_return_type_.has_value()) {
            if (!(ret->evaluated_type == *current_function_return_type_)) {
                emit_error(ret->location, "mismatched return type: expected '" +
                           current_function_return_type_->to_string() + "', found '" +
                           ret->evaluated_type.to_string() + "'");
            }
        }
        return ret->evaluated_type;
    }

    if (auto* block = dynamic_cast<BlockExpr*>(&expr)) {
        check_block(*block, Type{BaseTypeKind::Unit, "", false, false}, false);
        return block->evaluated_type;
    }

    if (auto* while_expr = dynamic_cast<WhileExpr*>(&expr)) {
        Type cond_t = check_expression(*while_expr->condition);
        if (cond_t.kind != BaseTypeKind::Bool) {
            emit_error(while_expr->condition->location, "mismatched types: expected 'bool', found '" +
                       cond_t.to_string() + "' in 'while' condition");
        }
        check_block(*while_expr->body, Type{BaseTypeKind::Unit, "", false, false}, false);
        while_expr->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
        return while_expr->evaluated_type;
    }

    if (auto* s_init = dynamic_cast<StructInitExpr*>(&expr)) {
        auto it = struct_table_.find(s_init->struct_name);
        if (it == struct_table_.end()) {
            emit_error(s_init->location, "cannot find struct '" + s_init->struct_name + "' in this scope");
            s_init->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return s_init->evaluated_type;
        }
        const auto& sig = it->second;
        std::map<std::string, Type> subst;
        if (!sig.type_params.empty()) {
            if (!s_init->type_args.empty()) {
                if (s_init->type_args.size() != sig.type_params.size()) {
                    emit_error(s_init->location, "struct '" + s_init->struct_name + "' takes " +
                               std::to_string(sig.type_params.size()) + " type arguments but " +
                               std::to_string(s_init->type_args.size()) + " were supplied");
                } else {
                    for (size_t i = 0; i < sig.type_params.size(); ++i) {
                        subst[sig.type_params[i]] = s_init->type_args[i];
                    }
                }
            }
        }

        std::set<std::string> provided_fields;
        for (auto& f : s_init->fields) {
            if (provided_fields.count(f.field_name)) {
                emit_error(f.location, "field '" + f.field_name + "' specified more than once");
            }
            provided_fields.insert(f.field_name);
            auto field_it = sig.fields.find(f.field_name);
            if (field_it == sig.fields.end()) {
                emit_error(f.location, "struct '" + s_init->struct_name + "' has no field named '" + f.field_name + "'");
            } else {
                Type val_t = check_expression(*f.value);
                if (!sig.type_params.empty()) {
                    unify_type(field_it->second, val_t, subst);
                }
            }
        }

        if (!sig.type_params.empty() && s_init->type_args.empty()) {
            for (const auto& tp : sig.type_params) {
                auto sit = subst.find(tp);
                if (sit != subst.end()) {
                    s_init->type_args.push_back(sit->second);
                } else {
                    emit_error(s_init->location, "cannot infer type argument for type parameter '" + tp +
                               "' in struct initializer of '" + s_init->struct_name + "'");
                }
            }
        }

        // Validate field types against substituted expected types
        for (auto& f : s_init->fields) {
            auto field_it = sig.fields.find(f.field_name);
            if (field_it != sig.fields.end()) {
                Type expected_t = substitute_type(field_it->second, subst);
                Type val_t = f.value->evaluated_type;
                if (!(val_t == expected_t)) {
                    emit_error(f.location, "mismatched type for field '" + f.field_name + "': expected '" +
                               expected_t.to_string() + "', found '" + val_t.to_string() + "'");
                }
            }
        }

        for (const auto& expected_field : sig.field_order) {
            if (!provided_fields.count(expected_field)) {
                emit_error(s_init->location, "missing field '" + expected_field + "' in initializer of '" + s_init->struct_name + "'");
            }
        }
        s_init->evaluated_type = Type{BaseTypeKind::Named, s_init->struct_name, false, false, s_init->type_args};
        return s_init->evaluated_type;
    }

    if (auto* fa = dynamic_cast<FieldAccessExpr*>(&expr)) {
        Type target_t = check_expression(*fa->target);
        if (target_t.kind != BaseTypeKind::Named) {
            emit_error(fa->location, "type '" + target_t.to_string() + "' does not have fields");
            fa->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return fa->evaluated_type;
        }
        auto it = struct_table_.find(target_t.name);
        if (it == struct_table_.end()) {
            emit_error(fa->location, "unknown struct '" + target_t.name + "'");
            fa->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return fa->evaluated_type;
        }
        const auto& sig = it->second;
        auto field_it = sig.fields.find(fa->field_name);
        if (field_it == sig.fields.end()) {
            emit_error(fa->location, "no field '" + fa->field_name + "' on type '" + target_t.name + "'");
            fa->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return fa->evaluated_type;
        }
        std::map<std::string, Type> subst;
        if (!sig.type_params.empty() && target_t.type_args.size() == sig.type_params.size()) {
            for (size_t i = 0; i < sig.type_params.size(); ++i) {
                subst[sig.type_params[i]] = target_t.type_args[i];
            }
        }
        fa->evaluated_type = substitute_type(field_it->second, subst);
        return fa->evaluated_type;
    }

    if (auto* mc = dynamic_cast<MethodCallExpr*>(&expr)) {
        Type receiver_t = check_expression(*mc->receiver);
        std::string base_struct = receiver_t.name;

        // Core standard library methods on Option and Result
        if (base_struct == "Option") {
            if (mc->method_name == "is_some" || mc->method_name == "is_none") {
                mc->resolved_self_kind = SelfKind::Ref;
                mc->resolved_callee = "Option__" + mc->method_name;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "unwrap") {
                mc->resolved_self_kind = SelfKind::Value;
                mc->resolved_callee = "Option__unwrap";
                mc->evaluated_type = receiver_t.type_args.empty() ? Type{BaseTypeKind::I32, false, false} : receiver_t.type_args[0];
                return mc->evaluated_type;
            }
        }
        if (base_struct == "Result") {
            if (mc->method_name == "is_ok" || mc->method_name == "is_err") {
                mc->resolved_self_kind = SelfKind::Ref;
                mc->resolved_callee = "Result__" + mc->method_name;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "unwrap") {
                mc->resolved_self_kind = SelfKind::Value;
                mc->resolved_callee = "Result__unwrap";
                mc->evaluated_type = receiver_t.type_args.empty() ? Type{BaseTypeKind::I32, false, false} : receiver_t.type_args[0];
                return mc->evaluated_type;
            }
        }

        if (base_struct == "VolatileRegister") {
            std::string mode = "ReadWrite";
            Type val_type{BaseTypeKind::I32, false, false};
            if (!receiver_t.type_args.empty()) {
                val_type = receiver_t.type_args[0];
            }
            if (receiver_t.type_args.size() > 1) {
                mode = receiver_t.type_args[1].name;
            }

            if (mc->method_name == "read") {
                if (mode == "WriteOnly") {
                    emit_error(mc->location, "cannot read from WriteOnly register: mode constraint violation");
                }
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "VolatileRegister::read takes 0 arguments");
                }
                mc->resolved_callee = "VolatileRegister__read";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = val_type;
                return mc->evaluated_type;
            }

            if (mc->method_name == "write") {
                if (mode == "ReadOnly") {
                    emit_error(mc->location, "cannot write to ReadOnly register: mode constraint violation");
                }
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "VolatileRegister::write takes exactly 1 argument");
                } else {
                    Type arg_t = check_expression(*mc->arguments[0]);
                    if (!(arg_t == val_type)) {
                        emit_error(mc->arguments[0]->location, "mismatched types in VolatileRegister::write: expected '" +
                                   val_type.to_string() + "', found '" + arg_t.to_string() + "'");
                    }
                }
                mc->resolved_callee = "VolatileRegister__write";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }

            if (mc->method_name == "addr" || mc->method_name == "address") {
                mc->resolved_callee = "VolatileRegister__addr";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "Q16_16") {
            if (mc->method_name == "to_int") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Q16_16::to_int takes 0 arguments");
                }
                mc->resolved_callee = "Q16_16__to_int";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "raw") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Q16_16::raw takes 0 arguments");
                }
                mc->resolved_callee = "Q16_16__raw";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "saturating_add" || mc->method_name == "saturating_sub" || mc->method_name == "saturating_mul") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "Q16_16::" + mc->method_name + " takes exactly 1 argument");
                } else {
                    Type arg_t = check_expression(*mc->arguments[0]);
                    if (arg_t.kind != BaseTypeKind::Named || arg_t.name != "Q16_16") {
                        emit_error(mc->arguments[0]->location, "mismatched types in Q16_16::" + mc->method_name + ": expected 'Q16_16', found '" + arg_t.to_string() + "'");
                    }
                }
                mc->resolved_callee = "Q16_16__" + mc->method_name;
                mc->resolved_self_kind = SelfKind::Value;
                mc->evaluated_type = Type{BaseTypeKind::Named, "Q16_16", false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "SlewRateLimiter") {
            if (mc->method_name == "step") {
                if (mc->arguments.size() != 2) {
                    emit_error(mc->location, "SlewRateLimiter::step takes exactly 2 arguments (target, dt_ms)");
                } else {
                    Type t_arg = check_expression(*mc->arguments[0]);
                    Type dt_arg = check_expression(*mc->arguments[1]);
                    if (t_arg.kind != BaseTypeKind::I32 || dt_arg.kind != BaseTypeKind::I32) {
                        emit_error(mc->location, "SlewRateLimiter::step arguments must be 'i32'");
                    }
                }
                mc->resolved_callee = "SlewRateLimiter__step";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "current") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SlewRateLimiter::current takes 0 arguments");
                }
                mc->resolved_callee = "SlewRateLimiter__current";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_saturated") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SlewRateLimiter::is_saturated takes 0 arguments");
                }
                mc->resolved_callee = "SlewRateLimiter__is_saturated";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
        }

        if (receiver_t.kind != BaseTypeKind::Named || (struct_table_.find(base_struct) == struct_table_.end() && enum_table_.find(base_struct) == enum_table_.end())) {
            emit_error(mc->location, "cannot call method '" + mc->method_name + "' on non-struct type '" + receiver_t.to_string() + "'");
            mc->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return mc->evaluated_type;
        }

        auto it = method_table_.find({base_struct, mc->method_name});
        if (it == method_table_.end()) {
            emit_error(mc->location, "no method named '" + mc->method_name + "' found for type '" + base_struct + "'");
            mc->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return mc->evaluated_type;
        }

        const auto& method_sig = it->second;
        if (method_sig.parameters.empty()) {
            emit_error(mc->location, "method '" + mc->method_name + "' on type '" + base_struct + "' is an associated function without a self receiver");
            mc->evaluated_type = method_sig.return_type;
            return mc->evaluated_type;
        }

        const auto& self_param = method_sig.parameters[0];
        SelfKind self_kind = SelfKind::Value;
        if (self_param.type.is_ref) {
            self_kind = self_param.type.is_mut_ref ? SelfKind::RefMut : SelfKind::Ref;
        }
        mc->resolved_self_kind = self_kind;
        mc->resolved_callee = method_sig.mangled_name;

        // Check mutability if method takes &mut self
        if (self_kind == SelfKind::RefMut) {
            if (auto* var_expr = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                auto var_info = lookup_variable(var_expr->name);
                if (var_info && !var_info->second && !receiver_t.is_mut_ref) {
                    emit_error(mc->location, "cannot borrow immutable local variable '" + var_expr->name + "' as mutable in method call");
                }
            }
            if (receiver_t.is_ref && !receiver_t.is_mut_ref) {
                emit_error(mc->location, "cannot borrow data in a '&' reference as mutable");
            }
        }

        size_t expected_args = method_sig.parameters.size() - 1;
        if (mc->arguments.size() != expected_args) {
            emit_error(mc->location, "method '" + mc->method_name + "' takes " +
                       std::to_string(expected_args) + " arguments but " +
                       std::to_string(mc->arguments.size()) + " were supplied");
            mc->evaluated_type = method_sig.return_type;
            return mc->evaluated_type;
        }

        for (size_t i = 0; i < mc->arguments.size(); ++i) {
            Type arg_t = check_expression(*mc->arguments[i]);
            Type exp_t = method_sig.parameters[i + 1].type;
            if (!(arg_t == exp_t)) {
                emit_error(mc->arguments[i]->location, "mismatched type in argument " + std::to_string(i + 1) +
                           " to method '" + mc->method_name + "': expected '" + exp_t.to_string() +
                           "', found '" + arg_t.to_string() + "'");
            }
        }

        mc->evaluated_type = method_sig.return_type;
        return mc->evaluated_type;
    }

    if (auto* ev = dynamic_cast<EnumVariantExpr*>(&expr)) {
        auto it = enum_table_.find(ev->enum_name);
        if (it == enum_table_.end()) {
            emit_error(ev->location, "cannot find enum '" + ev->enum_name + "' in this scope");
            ev->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return ev->evaluated_type;
        }
        const auto& sig = it->second;
        auto var_it = sig.variants.find(ev->variant_name);
        if (var_it == sig.variants.end()) {
            emit_error(ev->location, "enum '" + ev->enum_name + "' has no variant named '" + ev->variant_name + "'");
            ev->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return ev->evaluated_type;
        }
        const auto& var_sig = var_it->second;
        if (var_sig.payload_type.has_value()) {
            if (!ev->payload) {
                emit_error(ev->location, "variant '" + ev->enum_name + "::" + ev->variant_name + "' requires a payload of type '" +
                           var_sig.payload_type->to_string() + "'");
            } else {
                Type payload_t = check_expression(*ev->payload);
                if (!(payload_t == *var_sig.payload_type)) {
                    emit_error(ev->location, "mismatched payload type for variant '" + ev->enum_name + "::" + ev->variant_name +
                               "': expected '" + var_sig.payload_type->to_string() + "', found '" + payload_t.to_string() + "'");
                }
            }
        } else {
            if (ev->payload) {
                emit_error(ev->location, "variant '" + ev->enum_name + "::" + ev->variant_name + "' does not take a payload");
                check_expression(*ev->payload);
            }
        }
        ev->evaluated_type = Type{BaseTypeKind::Named, ev->enum_name, false, false};
        return ev->evaluated_type;
    }

    if (auto* match_expr = dynamic_cast<MatchExpr*>(&expr)) {
        Type target_t = check_expression(*match_expr->target);
        if (target_t.kind != BaseTypeKind::Named) {
            emit_error(match_expr->location, "cannot match on non-enum type '" + target_t.to_string() + "'");
            match_expr->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return match_expr->evaluated_type;
        }
        auto it = enum_table_.find(target_t.name);
        if (it == enum_table_.end()) {
            emit_error(match_expr->location, "cannot match on unknown enum '" + target_t.name + "'");
            match_expr->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return match_expr->evaluated_type;
        }
        const auto& enum_sig = it->second;
        std::set<std::string> covered_variants;
        bool has_wildcard = false;
        std::optional<Type> expected_arm_type;

        for (auto& arm : match_expr->arms) {
            enter_scope();
            if (arm.pattern.kind == PatternKind::Wildcard) {
                has_wildcard = true;
            } else {
                if (arm.pattern.enum_name != enum_sig.name) {
                    emit_error(arm.pattern.location, "mismatched enum in pattern: expected '" + enum_sig.name +
                               "', found '" + arm.pattern.enum_name + "'");
                }
                auto var_it = enum_sig.variants.find(arm.pattern.variant_name);
                if (var_it == enum_sig.variants.end()) {
                    emit_error(arm.pattern.location, "enum '" + enum_sig.name + "' has no variant named '" +
                               arm.pattern.variant_name + "'");
                } else {
                    const auto& var_sig = var_it->second;
                    if (var_sig.payload_type.has_value()) {
                        if (arm.pattern.binding_name.empty()) {
                            emit_error(arm.pattern.location, "pattern for variant '" + arm.pattern.variant_name +
                                       "' must bind payload variable");
                        } else {
                            declare_variable(arm.pattern.binding_name, *var_sig.payload_type, false, arm.pattern.location);
                        }
                    } else {
                        if (!arm.pattern.binding_name.empty()) {
                            emit_error(arm.pattern.location, "variant '" + arm.pattern.variant_name +
                                       "' has no payload to bind");
                        }
                    }
                    covered_variants.insert(arm.pattern.variant_name);
                }
            }

            Type arm_t = check_expression(*arm.body);
            exit_scope();

            if (!expected_arm_type.has_value()) {
                expected_arm_type = arm_t;
            } else if (!(*expected_arm_type == arm_t)) {
                emit_error(arm.location, "match arms have incompatible types: expected '" +
                           expected_arm_type->to_string() + "', found '" + arm_t.to_string() + "'");
            }
        }

        // Exhaustiveness check
        if (!has_wildcard) {
            for (const auto& [vname, _] : enum_sig.variants) {
                if (covered_variants.find(vname) == covered_variants.end()) {
                    emit_error(match_expr->location, "non-exhaustive patterns: variant '" + vname + "' not covered");
                }
            }
        }

        match_expr->evaluated_type = expected_arm_type.value_or(Type{BaseTypeKind::Unit, "", false, false});
        return match_expr->evaluated_type;
    }

    return Type{BaseTypeKind::Unit, "", false, false};
}

} // namespace oxide
