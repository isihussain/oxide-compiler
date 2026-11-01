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
    struct_table_["SafetyCorridor"] = StructSignature{"SafetyCorridor", {}, {
        {"min_x", Type{BaseTypeKind::I32, false, false}},
        {"max_x", Type{BaseTypeKind::I32, false, false}},
        {"min_y", Type{BaseTypeKind::I32, false, false}},
        {"max_y", Type{BaseTypeKind::I32, false, false}},
        {"min_z", Type{BaseTypeKind::I32, false, false}},
        {"max_z", Type{BaseTypeKind::I32, false, false}},
        {"max_velocity", Type{BaseTypeKind::I32, false, false}}
    }, {"min_x", "max_x", "min_y", "max_y", "min_z", "max_z", "max_velocity"}, SourceLocation{0, 0}};
    struct_table_["SpscRingBuffer"] = StructSignature{"SpscRingBuffer", {}, {
        {"head", Type{BaseTypeKind::I32, false, false}},
        {"tail", Type{BaseTypeKind::I32, false, false}},
        {"capacity", Type{BaseTypeKind::I32, false, false}}
    }, {"head", "tail", "capacity"}, SourceLocation{0, 0}};
    struct_table_["StageMass"] = StructSignature{"StageMass", {}, {
        {"dry_mass", Type{BaseTypeKind::I32, false, false}},
        {"propellant_mass", Type{BaseTypeKind::I32, false, false}}
    }, {"dry_mass", "propellant_mass"}, SourceLocation{0, 0}};
    struct_table_["TelemetryPacket"] = StructSignature{"TelemetryPacket", {}, {
        {"packet_id", Type{BaseTypeKind::I32, false, false}},
        {"timestamp", Type{BaseTypeKind::I32, false, false}},
        {"ch0", Type{BaseTypeKind::I32, false, false}},
        {"ch1", Type{BaseTypeKind::I32, false, false}},
        {"ch2", Type{BaseTypeKind::I32, false, false}},
        {"ch3", Type{BaseTypeKind::I32, false, false}}
    }, {"packet_id", "timestamp", "ch0", "ch1", "ch2", "ch3"}, SourceLocation{0, 0}};
    struct_table_["FlightTable"] = StructSignature{"FlightTable", {}, {
        {"active_val", Type{BaseTypeKind::I32, false, false}},
        {"staging_val", Type{BaseTypeKind::I32, false, false}},
        {"staging_loaded", Type{BaseTypeKind::Bool, false, false}},
        {"staging_verified", Type{BaseTypeKind::Bool, false, false}},
        {"active_checksum", Type{BaseTypeKind::I32, false, false}},
        {"staging_checksum", Type{BaseTypeKind::I32, false, false}},
        {"commit_count", Type{BaseTypeKind::I32, false, false}}
    }, {"active_val", "staging_val", "staging_loaded", "staging_verified", "active_checksum", "staging_checksum", "commit_count"}, SourceLocation{0, 0}};
    struct_table_["Quaternion"] = StructSignature{"Quaternion", {}, {
        {"w", Type{BaseTypeKind::I32, false, false}},
        {"x", Type{BaseTypeKind::I32, false, false}},
        {"y", Type{BaseTypeKind::I32, false, false}},
        {"z", Type{BaseTypeKind::I32, false, false}}
    }, {"w", "x", "y", "z"}, SourceLocation{0, 0}};
    struct_table_["SoftwareBus"] = StructSignature{"SoftwareBus", {}, {}, {}, SourceLocation{0, 0}};
    struct_table_["WatchdogMonitor"] = StructSignature{"WatchdogMonitor", {}, {}, {}, SourceLocation{0, 0}};
    struct_table_["SensorFDIR"] = StructSignature{"SensorFDIR", {}, {
        {"min_val", Type{BaseTypeKind::I32, false, false}},
        {"max_val", Type{BaseTypeKind::I32, false, false}},
        {"degrade_threshold", Type{BaseTypeKind::I32, false, false}},
        {"isolate_threshold", Type{BaseTypeKind::I32, false, false}},
        {"recovery_threshold", Type{BaseTypeKind::I32, false, false}},
        {"state", Type{BaseTypeKind::I32, false, false}},
        {"anomaly_count", Type{BaseTypeKind::I32, false, false}},
        {"healthy_count", Type{BaseTypeKind::I32, false, false}},
        {"last_valid_sample", Type{BaseTypeKind::I32, false, false}},
        {"total_anomalies", Type{BaseTypeKind::I32, false, false}}
    }, {"min_val", "max_val", "degrade_threshold", "isolate_threshold", "recovery_threshold", "state", "anomaly_count", "healthy_count", "last_valid_sample", "total_anomalies"}, SourceLocation{0, 0}};
    struct_table_["HoldDownInterlock"] = StructSignature{"HoldDownInterlock", {}, {
        {"mode", Type{BaseTypeKind::I32, false, false}},
        {"rated_clamp_strain", Type{BaseTypeKind::I32, false, false}},
        {"clamp_strain", Type{BaseTypeKind::I32, false, false}},
        {"accel_z", Type{BaseTypeKind::I32, false, false}},
        {"umbilical_connected", Type{BaseTypeKind::Bool, false, false}},
        {"thrust_terminated", Type{BaseTypeKind::Bool, false, false}},
        {"abort_reason", Type{BaseTypeKind::I32, false, false}}
    }, {"mode", "rated_clamp_strain", "clamp_strain", "accel_z", "umbilical_connected", "thrust_terminated", "abort_reason"}, SourceLocation{0, 0}};
    struct_table_["TurbopumpMonitor"] = StructSignature{"TurbopumpMonitor", {}, {
        {"min_rpm", Type{BaseTypeKind::I32, false, false}},
        {"max_rpm", Type{BaseTypeKind::I32, false, false}},
        {"min_delta_p", Type{BaseTypeKind::I32, false, false}},
        {"cavitation_limit", Type{BaseTypeKind::I32, false, false}},
        {"shaft_rpm", Type{BaseTypeKind::I32, false, false}},
        {"delta_p", Type{BaseTypeKind::I32, false, false}},
        {"cavitation_count", Type{BaseTypeKind::I32, false, false}},
        {"state", Type{BaseTypeKind::I32, false, false}},
        {"trip_code", Type{BaseTypeKind::I32, false, false}}
    }, {"min_rpm", "max_rpm", "min_delta_p", "cavitation_limit", "shaft_rpm", "delta_p", "cavitation_count", "state", "trip_code"}, SourceLocation{0, 0}};
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
        if (let->initializer) {
            if (auto* call = dynamic_cast<CallExpr*>(let->initializer.get())) {
                if (call->callee == "SafetyCorridor::new" && call->arguments.size() == 7) {
                    int64_t min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0, max_v = 0;
                    bool b1 = extract_const_int(call->arguments[0].get(), min_x);
                    bool b2 = extract_const_int(call->arguments[1].get(), max_x);
                    bool b3 = extract_const_int(call->arguments[2].get(), min_y);
                    bool b4 = extract_const_int(call->arguments[3].get(), max_y);
                    bool b5 = extract_const_int(call->arguments[4].get(), min_z);
                    bool b6 = extract_const_int(call->arguments[5].get(), max_z);
                    bool b7 = extract_const_int(call->arguments[6].get(), max_v);
                    if (b1 && b2 && b3 && b4 && b5 && b6 && b7) {
                        corridor_vars_[let->var_name] = SafetyCorridorBounds{min_x, max_x, min_y, max_y, min_z, max_z, max_v};
                    }
                }
                if (call->callee == "StageMass::new" && call->arguments.size() == 2) {
                    int64_t dry = 0, prop = 0;
                    bool b1 = extract_const_int(call->arguments[0].get(), dry);
                    bool b2 = extract_const_int(call->arguments[1].get(), prop);
                    if (b1 && b2) {
                        stage_vars_[let->var_name] = StageMassRecord{dry, prop};
                    }
                }
                if (call->callee == "FlightTable::new" && call->arguments.size() == 1) {
                    int64_t init_val = 0;
                    bool has_init = extract_const_int(call->arguments[0].get(), init_val);
                    table_vars_[let->var_name] = FlightTableRecord{
                        has_init ? init_val : 0,
                        0,
                        false,
                        false,
                        has_init ? (init_val ^ 0x5A5A5A5A) : 0,
                        0
                    };
                }
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

        if (call->callee == "SafetyCorridor::new") {
            if (call->arguments.size() != 7) {
                emit_error(call->location, "SafetyCorridor::new takes exactly 7 arguments (min_x, max_x, min_y, max_y, min_z, max_z, max_velocity)");
            } else {
                for (size_t i = 0; i < 7; ++i) {
                    Type t = check_expression(*call->arguments[i]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(call->arguments[i]->location, "SafetyCorridor parameter must be 'i32'");
                    }
                }
                int64_t min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0, max_v = 0;
                bool has_min_x = extract_const_int(call->arguments[0].get(), min_x);
                bool has_max_x = extract_const_int(call->arguments[1].get(), max_x);
                bool has_min_y = extract_const_int(call->arguments[2].get(), min_y);
                bool has_max_y = extract_const_int(call->arguments[3].get(), max_y);
                bool has_min_z = extract_const_int(call->arguments[4].get(), min_z);
                bool has_max_z = extract_const_int(call->arguments[5].get(), max_z);
                bool has_max_v = extract_const_int(call->arguments[6].get(), max_v);

                if (has_min_x && has_max_x && min_x >= max_x) {
                    emit_error(call->location, "invalid safety corridor: min_x (" +
                               std::to_string(min_x) + ") must be strictly less than max_x (" +
                               std::to_string(max_x) + ")");
                }
                if (has_min_y && has_max_y && min_y >= max_y) {
                    emit_error(call->location, "invalid safety corridor: min_y (" +
                               std::to_string(min_y) + ") must be strictly less than max_y (" +
                               std::to_string(max_y) + ")");
                }
                if (has_min_z && has_max_z && min_z >= max_z) {
                    emit_error(call->location, "invalid safety corridor: min_z (" +
                               std::to_string(min_z) + ") must be strictly less than max_z (" +
                               std::to_string(max_z) + ")");
                }
                if (has_max_v && max_v <= 0) {
                    emit_error(call->location, "invalid safety corridor: max_velocity (" +
                               std::to_string(max_v) + ") must be positive");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "SafetyCorridor", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "SpscRingBuffer::new") {
            if (call->arguments.size() != 1) {
                emit_error(call->location, "SpscRingBuffer::new takes exactly 1 argument (capacity)");
            } else {
                Type t = check_expression(*call->arguments[0]);
                if (t.kind != BaseTypeKind::I32) {
                    emit_error(call->arguments[0]->location, "SpscRingBuffer capacity must be 'i32'");
                }
                int64_t cap = 0;
                if (extract_const_int(call->arguments[0].get(), cap)) {
                    if (cap <= 0 || cap > 64) {
                        emit_error(call->location, "invalid SpscRingBuffer capacity (" +
                                   std::to_string(cap) + "): must be between 1 and 64");
                    }
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "SpscRingBuffer", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "StageMass::new") {
            if (call->arguments.size() != 2) {
                emit_error(call->location, "StageMass::new takes exactly 2 arguments (dry_mass, initial_propellant)");
            } else {
                for (size_t i = 0; i < 2; ++i) {
                    Type t = check_expression(*call->arguments[i]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(call->arguments[i]->location, "StageMass parameter must be 'i32'");
                    }
                }
                int64_t dry = 0, prop = 0;
                bool has_dry = extract_const_int(call->arguments[0].get(), dry);
                bool has_prop = extract_const_int(call->arguments[1].get(), prop);
                if (has_dry && dry <= 0) {
                    emit_error(call->location, "invalid stage mass configuration: dry_mass (" +
                               std::to_string(dry) + ") must be positive");
                }
                if (has_prop && prop < 0) {
                    emit_error(call->location, "invalid stage mass configuration: initial_propellant (" +
                               std::to_string(prop) + ") cannot be negative");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "StageMass", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "TelemetryPacket::new") {
            if (call->arguments.size() != 6) {
                emit_error(call->location, "TelemetryPacket::new takes exactly 6 arguments (packet_id, timestamp, ch0, ch1, ch2, ch3)");
            } else {
                for (size_t i = 0; i < 6; ++i) {
                    Type t = check_expression(*call->arguments[i]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(call->arguments[i]->location, "TelemetryPacket parameter must be 'i32'");
                    }
                }
                int64_t id = 0, ts = 0;
                bool has_id = extract_const_int(call->arguments[0].get(), id);
                bool has_ts = extract_const_int(call->arguments[1].get(), ts);
                if (has_id && id <= 0) {
                    emit_error(call->location, "invalid telemetry packet: packet_id (" +
                               std::to_string(id) + ") must be positive");
                }
                if (has_ts && ts < 0) {
                    emit_error(call->location, "invalid telemetry packet: timestamp (" +
                               std::to_string(ts) + ") cannot be negative");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "TelemetryPacket", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "FlightTable::new") {
            if (call->arguments.size() != 1) {
                emit_error(call->location, "FlightTable::new takes exactly 1 argument (initial_parameter)");
            } else {
                Type t = check_expression(*call->arguments[0]);
                if (t.kind != BaseTypeKind::I32) {
                    emit_error(call->arguments[0]->location, "FlightTable initial parameter must be 'i32'");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "FlightTable", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "FlightTable::compute_checksum") {
            if (call->arguments.size() != 1) {
                emit_error(call->location, "FlightTable::compute_checksum takes exactly 1 argument (val)");
            } else {
                Type t = check_expression(*call->arguments[0]);
                if (t.kind != BaseTypeKind::I32) {
                    emit_error(call->arguments[0]->location, "FlightTable::compute_checksum argument must be 'i32'");
                }
            }
            call->callee = "FlightTable__compute_checksum";
            call->evaluated_type = Type{BaseTypeKind::I32, false, false};
            return call->evaluated_type;
        }

        if (call->callee == "Quaternion::new") {
            if (call->arguments.size() != 4) {
                emit_error(call->location, "Quaternion::new takes exactly 4 arguments (w, x, y, z)");
            } else {
                for (size_t i = 0; i < 4; ++i) {
                    Type t = check_expression(*call->arguments[i]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(call->arguments[i]->location, "Quaternion coordinate must be 'i32'");
                    }
                }
                int64_t w = 0, x = 0, y = 0, z = 0;
                bool bw = extract_const_int(call->arguments[0].get(), w);
                bool bx = extract_const_int(call->arguments[1].get(), x);
                bool by = extract_const_int(call->arguments[2].get(), y);
                bool bz = extract_const_int(call->arguments[3].get(), z);
                if (bw && bx && by && bz) {
                    int64_t norm_sq = w*w + x*x + y*y + z*z;
                    int64_t diff = std::abs(norm_sq - 1000000);
                    if (diff > 2000) {
                        emit_error(call->location, "invalid unit quaternion literal: norm squared (" +
                                   std::to_string(norm_sq) + ") deviates from unit scale (1000000) [NASA ADCS SOHO rule: |q|^2 must equal 1.0 within tolerance]");
                    }
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "Quaternion", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "Quaternion::identity") {
            if (!call->arguments.empty()) {
                emit_error(call->location, "Quaternion::identity takes 0 arguments");
            }
            call->callee = "Quaternion__identity";
            call->evaluated_type = Type{BaseTypeKind::Named, "Quaternion", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "SoftwareBus::new") {
            if (!call->arguments.empty()) {
                emit_error(call->location, "SoftwareBus::new takes 0 arguments");
            }
            call->callee = "SoftwareBus__new";
            call->evaluated_type = Type{BaseTypeKind::Named, "SoftwareBus", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "WatchdogMonitor::new") {
            if (!call->arguments.empty()) {
                emit_error(call->location, "WatchdogMonitor::new takes 0 arguments");
            }
            call->callee = "WatchdogMonitor__new";
            call->evaluated_type = Type{BaseTypeKind::Named, "WatchdogMonitor", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "SensorFDIR::new") {
            if (call->arguments.size() != 5) {
                emit_error(call->location, "SensorFDIR::new takes exactly 5 arguments (min_val, max_val, degrade_threshold, isolate_threshold, recovery_threshold)");
            } else {
                for (size_t i = 0; i < 5; ++i) {
                    Type t = check_expression(*call->arguments[i]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(call->arguments[i]->location, "SensorFDIR parameter must be 'i32'");
                    }
                }
                int64_t min_v = 0, max_v = 0, deg = 0, iso = 0, rec = 0;
                bool b_min = extract_const_int(call->arguments[0].get(), min_v);
                bool b_max = extract_const_int(call->arguments[1].get(), max_v);
                bool b_deg = extract_const_int(call->arguments[2].get(), deg);
                bool b_iso = extract_const_int(call->arguments[3].get(), iso);
                bool b_rec = extract_const_int(call->arguments[4].get(), rec);

                if (b_min && b_max && min_v > max_v) {
                    emit_error(call->location, "invalid sensor FDIR bounds: min_val (" + std::to_string(min_v) +
                               ") exceeds max_val (" + std::to_string(max_v) + ")");
                }
                if (b_deg && deg <= 0) {
                    emit_error(call->location, "invalid sensor FDIR degrade_threshold (" + std::to_string(deg) +
                               "): must be positive");
                }
                if (b_deg && b_iso && iso <= deg) {
                    emit_error(call->location, "invalid sensor FDIR thresholds: isolate_threshold (" + std::to_string(iso) +
                               ") must exceed degrade_threshold (" + std::to_string(deg) + ")");
                }
                if (b_rec && rec <= 0) {
                    emit_error(call->location, "invalid sensor FDIR recovery_threshold (" + std::to_string(rec) +
                               "): must be positive");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "SensorFDIR", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "HoldDownInterlock::new") {
            if (call->arguments.size() != 2) {
                emit_error(call->location, "HoldDownInterlock::new takes exactly 2 arguments (mode, rated_clamp_strain)");
            } else {
                Type t0 = check_expression(*call->arguments[0]);
                Type t1 = check_expression(*call->arguments[1]);
                if (t0.kind != BaseTypeKind::I32 || t1.kind != BaseTypeKind::I32) {
                    emit_error(call->location, "HoldDownInterlock parameters must be 'i32'");
                }
                int64_t mode_val = 0;
                int64_t strain_val = 0;
                bool has_mode = extract_const_int(call->arguments[0].get(), mode_val);
                bool has_strain = extract_const_int(call->arguments[1].get(), strain_val);
                if (has_mode && mode_val != 0 && mode_val != 1) {
                    emit_error(call->location, "invalid hold down mode (" + std::to_string(mode_val) +
                               "): expected 0 (StaticFire) or 1 (FlightLaunch)");
                }
                if (has_strain && strain_val <= 0) {
                    emit_error(call->location, "invalid rated clamp strain (" + std::to_string(strain_val) +
                               "): must be positive");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "HoldDownInterlock", false, false};
            return call->evaluated_type;
        }

        if (call->callee == "TurbopumpMonitor::new") {
            if (call->arguments.size() != 4) {
                emit_error(call->location, "TurbopumpMonitor::new takes exactly 4 arguments (min_rpm, max_rpm, min_delta_p, cavitation_limit)");
            } else {
                for (size_t i = 0; i < 4; ++i) {
                    Type t = check_expression(*call->arguments[i]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(call->arguments[i]->location, "TurbopumpMonitor parameters must be 'i32'");
                    }
                }
                int64_t min_rpm_v = 0, max_rpm_v = 0, min_dp_v = 0, cav_lim_v = 0;
                bool b_min = extract_const_int(call->arguments[0].get(), min_rpm_v);
                bool b_max = extract_const_int(call->arguments[1].get(), max_rpm_v);
                bool b_dp = extract_const_int(call->arguments[2].get(), min_dp_v);
                bool b_lim = extract_const_int(call->arguments[3].get(), cav_lim_v);

                if (b_min && min_rpm_v <= 0) {
                    emit_error(call->location, "invalid turbopump min_rpm (" + std::to_string(min_rpm_v) + "): must be positive");
                }
                if (b_min && b_max && max_rpm_v <= min_rpm_v) {
                    emit_error(call->location, "invalid turbopump RPM bounds: max_rpm (" + std::to_string(max_rpm_v) +
                               ") must exceed min_rpm (" + std::to_string(min_rpm_v) + ")");
                }
                if (b_dp && min_dp_v <= 0) {
                    emit_error(call->location, "invalid turbopump min_delta_p (" + std::to_string(min_dp_v) + "): must be positive");
                }
                if (b_lim && cav_lim_v <= 0) {
                    emit_error(call->location, "invalid turbopump cavitation_limit (" + std::to_string(cav_lim_v) + "): must be positive");
                }
            }
            call->evaluated_type = Type{BaseTypeKind::Named, "TurbopumpMonitor", false, false};
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

        if (base_struct == "SafetyCorridor") {
            if (mc->method_name == "is_inside") {
                if (mc->arguments.size() != 3) {
                    emit_error(mc->location, "SafetyCorridor::is_inside takes exactly 3 arguments (x, y, z)");
                } else {
                    for (size_t i = 0; i < 3; ++i) {
                        Type t = check_expression(*mc->arguments[i]);
                        if (t.kind != BaseTypeKind::I32) {
                            emit_error(mc->arguments[i]->location, "SafetyCorridor::is_inside arguments must be 'i32'");
                        }
                    }
                }
                mc->resolved_callee = "SafetyCorridor__is_inside";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "check") {
                if (mc->arguments.size() != 6) {
                    emit_error(mc->location, "SafetyCorridor::check takes exactly 6 arguments (x, y, z, vx, vy, vz)");
                } else {
                    for (size_t i = 0; i < 6; ++i) {
                        Type t = check_expression(*mc->arguments[i]);
                        if (t.kind != BaseTypeKind::I32) {
                            emit_error(mc->arguments[i]->location, "SafetyCorridor::check arguments must be 'i32'");
                        }
                    }
                }
                mc->resolved_callee = "SafetyCorridor__check";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "verify_or_terminate") {
                if (mc->arguments.size() != 6) {
                    emit_error(mc->location, "SafetyCorridor::verify_or_terminate takes exactly 6 arguments (x, y, z, vx, vy, vz)");
                } else {
                    for (size_t i = 0; i < 6; ++i) {
                        Type t = check_expression(*mc->arguments[i]);
                        if (t.kind != BaseTypeKind::I32) {
                            emit_error(mc->arguments[i]->location, "SafetyCorridor::verify_or_terminate arguments must be 'i32'");
                        }
                    }

                    if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                        auto it = corridor_vars_.find(var->name);
                        if (it != corridor_vars_.end()) {
                            const auto& bounds = it->second;
                            int64_t x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0;
                            bool has_x = extract_const_int(mc->arguments[0].get(), x);
                            bool has_y = extract_const_int(mc->arguments[1].get(), y);
                            bool has_z = extract_const_int(mc->arguments[2].get(), z);
                            bool has_vx = extract_const_int(mc->arguments[3].get(), vx);
                            bool has_vy = extract_const_int(mc->arguments[4].get(), vy);
                            bool has_vz = extract_const_int(mc->arguments[5].get(), vz);

                            if (has_x && (x < bounds.min_x || x > bounds.max_x)) {
                                emit_error(mc->location, "AFTS safety corridor breach detected: position x (" +
                                           std::to_string(x) + ") exceeds corridor limits [" +
                                           std::to_string(bounds.min_x) + ", " + std::to_string(bounds.max_x) + "]");
                            } else if (has_y && (y < bounds.min_y || y > bounds.max_y)) {
                                emit_error(mc->location, "AFTS safety corridor breach detected: position y (" +
                                           std::to_string(y) + ") exceeds corridor limits [" +
                                           std::to_string(bounds.min_y) + ", " + std::to_string(bounds.max_y) + "]");
                            } else if (has_z && (z < bounds.min_z || z > bounds.max_z)) {
                                emit_error(mc->location, "AFTS safety corridor breach detected: position z (" +
                                           std::to_string(z) + ") exceeds corridor limits [" +
                                           std::to_string(bounds.min_z) + ", " + std::to_string(bounds.max_z) + "]");
                            } else if (has_vx && has_vy && has_vz) {
                                int64_t v2 = vx * vx + vy * vy + vz * vz;
                                int64_t max_v2 = bounds.max_velocity * bounds.max_velocity;
                                if (v2 > max_v2) {
                                    emit_error(mc->location, "AFTS safety corridor breach detected: velocity magnitude squared (" +
                                               std::to_string(v2) + ") exceeds maximum limit (" +
                                               std::to_string(max_v2) + ")");
                                }
                            }
                        }
                    }
                }
                mc->resolved_callee = "SafetyCorridor__verify_or_terminate";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "SpscRingBuffer") {
            if (mc->method_name == "push") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SpscRingBuffer::push takes exactly 1 argument (item)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SpscRingBuffer::push argument must be 'i32'");
                    }
                }
                mc->resolved_callee = "SpscRingBuffer__push";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "pop") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SpscRingBuffer::pop takes 0 arguments");
                }
                mc->resolved_callee = "SpscRingBuffer__pop";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_empty") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SpscRingBuffer::is_empty takes 0 arguments");
                }
                mc->resolved_callee = "SpscRingBuffer__is_empty";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_full") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SpscRingBuffer::is_full takes 0 arguments");
                }
                mc->resolved_callee = "SpscRingBuffer__is_full";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "len") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SpscRingBuffer::len takes 0 arguments");
                }
                mc->resolved_callee = "SpscRingBuffer__len";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "capacity") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SpscRingBuffer::capacity takes 0 arguments");
                }
                mc->resolved_callee = "SpscRingBuffer__capacity";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "StageMass") {
            if (mc->method_name == "dry_mass") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "StageMass::dry_mass takes 0 arguments");
                }
                mc->resolved_callee = "StageMass__dry_mass";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "propellant_mass") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "StageMass::propellant_mass takes 0 arguments");
                }
                mc->resolved_callee = "StageMass__propellant_mass";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "total_mass") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "StageMass::total_mass takes 0 arguments");
                }
                mc->resolved_callee = "StageMass__total_mass";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_depleted") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "StageMass::is_depleted takes 0 arguments");
                }
                mc->resolved_callee = "StageMass__is_depleted";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "burn") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "StageMass::burn takes exactly 1 argument (mass_burned)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "StageMass::burn argument must be 'i32'");
                    }

                    int64_t burned = 0;
                    if (extract_const_int(mc->arguments[0].get(), burned)) {
                        if (burned < 0) {
                            emit_error(mc->location, "cannot burn negative propellant mass (" +
                                       std::to_string(burned) + ")");
                        } else if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                            auto it = stage_vars_.find(var->name);
                            if (it != stage_vars_.end()) {
                                if (burned > it->second.propellant_mass) {
                                    emit_error(mc->location, "physical dry mass invariant violated: burn amount (" +
                                               std::to_string(burned) + ") exceeds available propellant (" +
                                               std::to_string(it->second.propellant_mass) + ")");
                                } else {
                                    it->second.propellant_mass -= burned;
                                }
                            }
                        }
                    }
                }
                mc->resolved_callee = "StageMass__burn";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "TelemetryPacket") {
            if (mc->method_name == "packet_id") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TelemetryPacket::packet_id takes 0 arguments");
                }
                mc->resolved_callee = "TelemetryPacket__packet_id";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "timestamp") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TelemetryPacket::timestamp takes 0 arguments");
                }
                mc->resolved_callee = "TelemetryPacket__timestamp";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "channel") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "TelemetryPacket::channel takes exactly 1 argument (index)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "TelemetryPacket::channel argument must be 'i32'");
                    }
                    int64_t idx = 0;
                    if (extract_const_int(mc->arguments[0].get(), idx)) {
                        if (idx < 0 || idx > 3) {
                            emit_error(mc->location, "telemetry packet channel index (" +
                                       std::to_string(idx) + ") out of bounds [0..3]");
                        }
                    }
                }
                mc->resolved_callee = "TelemetryPacket__channel";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "compute_crc") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TelemetryPacket::compute_crc takes 0 arguments");
                }
                mc->resolved_callee = "TelemetryPacket__compute_crc";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "verify_crc") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "TelemetryPacket::verify_crc takes exactly 1 argument (expected_crc)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "TelemetryPacket::verify_crc argument must be 'i32'");
                    }
                }
                mc->resolved_callee = "TelemetryPacket__verify_crc";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "verify_or_abort") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "TelemetryPacket::verify_or_abort takes exactly 1 argument (expected_crc)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "TelemetryPacket::verify_or_abort argument must be 'i32'");
                    }
                }
                mc->resolved_callee = "TelemetryPacket__verify_or_abort";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "FlightTable") {
            if (mc->method_name == "load_staging") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "FlightTable::load_staging takes exactly 1 argument (new_val)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "FlightTable::load_staging argument must be 'i32'");
                    }
                    if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                        auto it = table_vars_.find(var->name);
                        if (it != table_vars_.end()) {
                            it->second.staging_loaded = true;
                            it->second.staging_verified = false;
                            int64_t val = 0;
                            if (extract_const_int(mc->arguments[0].get(), val)) {
                                it->second.staging_val = val;
                                it->second.staging_checksum = val ^ 0x5A5A5A5A;
                            }
                        }
                    }
                }
                mc->resolved_callee = "FlightTable__load_staging";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "verify_staging") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "FlightTable::verify_staging takes exactly 1 argument (expected_checksum)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "FlightTable::verify_staging argument must be 'i32'");
                    }
                    if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                        auto it = table_vars_.find(var->name);
                        if (it != table_vars_.end()) {
                            int64_t exp = 0;
                            if (extract_const_int(mc->arguments[0].get(), exp)) {
                                if (it->second.staging_loaded && exp != it->second.staging_checksum) {
                                    emit_error(mc->location, "flight table verification failed: checksum mismatch (expected " +
                                               std::to_string(exp) + ", computed " + std::to_string(it->second.staging_checksum) + ")");
                                } else {
                                    it->second.staging_verified = true;
                                }
                            } else {
                                it->second.staging_verified = true;
                            }
                        }
                    }
                }
                mc->resolved_callee = "FlightTable__verify_staging";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "validate_bounds") {
                if (mc->arguments.size() != 2) {
                    emit_error(mc->location, "FlightTable::validate_bounds takes exactly 2 arguments (min_val, max_val)");
                } else {
                    Type t1 = check_expression(*mc->arguments[0]);
                    Type t2 = check_expression(*mc->arguments[1]);
                    if (t1.kind != BaseTypeKind::I32 || t2.kind != BaseTypeKind::I32) {
                        emit_error(mc->location, "FlightTable::validate_bounds arguments must be 'i32'");
                    }
                    int64_t mn = 0, mx = 0;
                    bool bmn = extract_const_int(mc->arguments[0].get(), mn);
                    bool bmx = extract_const_int(mc->arguments[1].get(), mx);
                    if (bmn && bmx && mn > mx) {
                        emit_error(mc->location, "invalid flight table bounds: min_val (" + std::to_string(mn) +
                                   ") exceeds max_val (" + std::to_string(mx) + ")");
                    }
                    if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                        auto it = table_vars_.find(var->name);
                        if (it != table_vars_.end() && it->second.staging_loaded) {
                            if (bmn && bmx) {
                                if (it->second.staging_val < mn || it->second.staging_val > mx) {
                                    emit_error(mc->location, "flight table parameter out of bounds: staging value (" +
                                               std::to_string(it->second.staging_val) + ") outside valid range [" +
                                               std::to_string(mn) + ", " + std::to_string(mx) + "]");
                                } else {
                                    it->second.staging_verified = true;
                                }
                            } else {
                                it->second.staging_verified = true;
                            }
                        }
                    }
                }
                mc->resolved_callee = "FlightTable__validate_bounds";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "commit") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::commit takes 0 arguments");
                }
                if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                    auto it = table_vars_.find(var->name);
                    if (it != table_vars_.end()) {
                        if (!it->second.staging_loaded) {
                            emit_error(mc->location, "invalid flight table commit: staging buffer is empty");
                        } else if (!it->second.staging_verified) {
                            emit_error(mc->location, "unverified flight table commit: staging buffer has not been verified prior to commit");
                        } else {
                            it->second.active_val = it->second.staging_val;
                            it->second.active_checksum = it->second.staging_checksum;
                            it->second.staging_loaded = false;
                            it->second.staging_verified = false;
                            it->second.staging_val = 0;
                        }
                    }
                }
                mc->resolved_callee = "FlightTable__commit";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "commit_or_abort") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::commit_or_abort takes 0 arguments");
                }
                if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                    auto it = table_vars_.find(var->name);
                    if (it != table_vars_.end()) {
                        if (!it->second.staging_loaded) {
                            emit_error(mc->location, "invalid flight table commit: staging buffer is empty");
                        } else if (!it->second.staging_verified) {
                            emit_error(mc->location, "unverified flight table commit: staging buffer has not been verified prior to commit");
                        } else {
                            it->second.active_val = it->second.staging_val;
                            it->second.active_checksum = it->second.staging_checksum;
                            it->second.staging_loaded = false;
                            it->second.staging_verified = false;
                            it->second.staging_val = 0;
                        }
                    }
                }
                mc->resolved_callee = "FlightTable__commit_or_abort";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "rollback") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::rollback takes 0 arguments");
                }
                if (auto* var = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                    auto it = table_vars_.find(var->name);
                    if (it != table_vars_.end()) {
                        it->second.staging_loaded = false;
                        it->second.staging_verified = false;
                        it->second.staging_val = 0;
                    }
                }
                mc->resolved_callee = "FlightTable__rollback";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "active") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::active takes 0 arguments");
                }
                mc->resolved_callee = "FlightTable__active";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "staging") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::staging takes 0 arguments");
                }
                mc->resolved_callee = "FlightTable__staging";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_staging_valid") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::is_staging_valid takes 0 arguments");
                }
                mc->resolved_callee = "FlightTable__is_staging_valid";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_staging_loaded") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::is_staging_loaded takes 0 arguments");
                }
                mc->resolved_callee = "FlightTable__is_staging_loaded";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "commit_count") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::commit_count takes 0 arguments");
                }
                mc->resolved_callee = "FlightTable__commit_count";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "active_checksum") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::active_checksum takes 0 arguments");
                }
                mc->resolved_callee = "FlightTable__active_checksum";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "staging_checksum") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "FlightTable::staging_checksum takes 0 arguments");
                }
                mc->resolved_callee = "FlightTable__staging_checksum";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "Quaternion") {
            if (mc->method_name == "w" || mc->method_name == "x" || mc->method_name == "y" || mc->method_name == "z") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Quaternion::" + mc->method_name + " takes 0 arguments");
                }
                mc->resolved_callee = "Quaternion__" + mc->method_name;
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "norm_squared") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Quaternion::norm_squared takes 0 arguments");
                }
                mc->resolved_callee = "Quaternion__norm_squared";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_normalized") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Quaternion::is_normalized takes 0 arguments");
                }
                mc->resolved_callee = "Quaternion__is_normalized";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_drifted") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Quaternion::is_drifted takes 0 arguments");
                }
                mc->resolved_callee = "Quaternion__is_drifted";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "renormalize") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Quaternion::renormalize takes 0 arguments");
                }
                mc->resolved_callee = "Quaternion__renormalize";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "conjugate") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "Quaternion::conjugate takes 0 arguments");
                }
                mc->resolved_callee = "Quaternion__conjugate";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Named, "Quaternion", false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "multiply") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "Quaternion::multiply takes exactly 1 argument (other)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::Named || t.name != "Quaternion") {
                        emit_error(mc->arguments[0]->location, "Quaternion::multiply argument must be a 'Quaternion'");
                    }
                }
                mc->resolved_callee = "Quaternion__multiply";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Named, "Quaternion", false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "inject_drift") {
                if (mc->arguments.size() != 4) {
                    emit_error(mc->location, "Quaternion::inject_drift takes exactly 4 arguments (dw, dx, dy, dz)");
                } else {
                    for (size_t i = 0; i < 4; ++i) {
                        Type t = check_expression(*mc->arguments[i]);
                        if (t.kind != BaseTypeKind::I32) {
                            emit_error(mc->arguments[i]->location, "Quaternion::inject_drift coordinate delta must be 'i32'");
                        }
                    }
                }
                mc->resolved_callee = "Quaternion__inject_drift";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "SoftwareBus") {
            if (mc->method_name == "subscribe") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SoftwareBus::subscribe takes exactly 1 argument (msg_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SoftwareBus::subscribe msg_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__subscribe";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_subscribed") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SoftwareBus::is_subscribed takes exactly 1 argument (msg_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SoftwareBus::is_subscribed msg_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__is_subscribed";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "publish") {
                if (mc->arguments.size() != 2) {
                    emit_error(mc->location, "SoftwareBus::publish takes exactly 2 arguments (msg_id, payload)");
                } else {
                    Type t1 = check_expression(*mc->arguments[0]);
                    Type t2 = check_expression(*mc->arguments[1]);
                    if (t1.kind != BaseTypeKind::I32 || t2.kind != BaseTypeKind::I32) {
                        emit_error(mc->location, "SoftwareBus::publish arguments must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__publish";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "receive") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SoftwareBus::receive takes exactly 1 argument (msg_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SoftwareBus::receive msg_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__receive";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "has_pending") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SoftwareBus::has_pending takes exactly 1 argument (msg_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SoftwareBus::has_pending msg_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__has_pending";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "pending_count") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SoftwareBus::pending_count takes exactly 1 argument (msg_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SoftwareBus::pending_count msg_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__pending_count";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "dropped_count") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SoftwareBus::dropped_count takes exactly 1 argument (msg_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SoftwareBus::dropped_count msg_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__dropped_count";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "clear") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SoftwareBus::clear takes exactly 1 argument (msg_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SoftwareBus::clear msg_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 8) {
                            emit_error(mc->location, "invalid software bus message ID (" + std::to_string(id) + "): must be within static channel range [0..7]");
                        }
                    }
                }
                mc->resolved_callee = "SoftwareBus__clear";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "WatchdogMonitor") {
            if (mc->method_name == "register_task") {
                if (mc->arguments.size() != 2) {
                    emit_error(mc->location, "WatchdogMonitor::register_task takes exactly 2 arguments (task_id, deadline_ticks)");
                } else {
                    Type t1 = check_expression(*mc->arguments[0]);
                    Type t2 = check_expression(*mc->arguments[1]);
                    if (t1.kind != BaseTypeKind::I32 || t2.kind != BaseTypeKind::I32) {
                        emit_error(mc->location, "WatchdogMonitor::register_task arguments must be 'i32'");
                    }
                    int64_t id = 0, dl = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 4) {
                            emit_error(mc->location, "invalid watchdog task ID (" + std::to_string(id) + "): must be within range [0..3]");
                        }
                    }
                    if (extract_const_int(mc->arguments[1].get(), dl)) {
                        if (dl <= 0) {
                            emit_error(mc->location, "invalid watchdog deadline (" + std::to_string(dl) + "): deadline_ticks must be strictly positive");
                        }
                    }
                }
                mc->resolved_callee = "WatchdogMonitor__register_task";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "strobe") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "WatchdogMonitor::strobe takes exactly 1 argument (task_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "WatchdogMonitor::strobe task_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 4) {
                            emit_error(mc->location, "invalid watchdog task ID (" + std::to_string(id) + "): must be within range [0..3]");
                        }
                    }
                }
                mc->resolved_callee = "WatchdogMonitor__strobe";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "tick") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "WatchdogMonitor::tick takes exactly 1 argument (elapsed_ticks)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "WatchdogMonitor::tick elapsed_ticks must be 'i32'");
                    }
                    int64_t ticks = 0;
                    if (extract_const_int(mc->arguments[0].get(), ticks)) {
                        if (ticks < 0) {
                            emit_error(mc->location, "invalid watchdog tick increment (" + std::to_string(ticks) + "): cannot be negative");
                        }
                    }
                }
                mc->resolved_callee = "WatchdogMonitor__tick";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_tripped") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "WatchdogMonitor::is_tripped takes 0 arguments");
                }
                mc->resolved_callee = "WatchdogMonitor__is_tripped";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "tripped_task") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "WatchdogMonitor::tripped_task takes 0 arguments");
                }
                mc->resolved_callee = "WatchdogMonitor__tripped_task";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "strobe_count") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "WatchdogMonitor::strobe_count takes exactly 1 argument (task_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "WatchdogMonitor::strobe_count task_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 4) {
                            emit_error(mc->location, "invalid watchdog task ID (" + std::to_string(id) + "): must be within range [0..3]");
                        }
                    }
                }
                mc->resolved_callee = "WatchdogMonitor__strobe_count";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_task_healthy") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "WatchdogMonitor::is_task_healthy takes exactly 1 argument (task_id)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "WatchdogMonitor::is_task_healthy task_id must be 'i32'");
                    }
                    int64_t id = 0;
                    if (extract_const_int(mc->arguments[0].get(), id)) {
                        if (id < 0 || id >= 4) {
                            emit_error(mc->location, "invalid watchdog task ID (" + std::to_string(id) + "): must be within range [0..3]");
                        }
                    }
                }
                mc->resolved_callee = "WatchdogMonitor__is_task_healthy";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "reset_trip") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "WatchdogMonitor::reset_trip takes 0 arguments");
                }
                mc->resolved_callee = "WatchdogMonitor__reset_trip";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "SensorFDIR") {
            if (mc->method_name == "update") {
                if (mc->arguments.size() != 1) {
                    emit_error(mc->location, "SensorFDIR::update takes exactly 1 argument (raw_sample)");
                } else {
                    Type t = check_expression(*mc->arguments[0]);
                    if (t.kind != BaseTypeKind::I32) {
                        emit_error(mc->arguments[0]->location, "SensorFDIR::update raw_sample must be 'i32'");
                    }
                }
                mc->resolved_callee = "SensorFDIR__update";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "state") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::state takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__state";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_healthy") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::is_healthy takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__is_healthy";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_degraded") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::is_degraded takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__is_degraded";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_isolated") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::is_isolated takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__is_isolated";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "anomaly_count") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::anomaly_count takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__anomaly_count";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "total_anomalies") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::total_anomalies takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__total_anomalies";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "last_valid") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::last_valid takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__last_valid";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "reset") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "SensorFDIR::reset takes 0 arguments");
                }
                mc->resolved_callee = "SensorFDIR__reset";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Unit, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "HoldDownInterlock") {
            if (mc->method_name == "update") {
                if (mc->arguments.size() != 3) {
                    emit_error(mc->location, "HoldDownInterlock::update takes exactly 3 arguments (clamp_strain, accel_z, umbilical_connected)");
                } else {
                    Type t0 = check_expression(*mc->arguments[0]);
                    Type t1 = check_expression(*mc->arguments[1]);
                    Type t2 = check_expression(*mc->arguments[2]);
                    if (t0.kind != BaseTypeKind::I32 || t1.kind != BaseTypeKind::I32) {
                        emit_error(mc->location, "HoldDownInterlock::update strain and accel must be 'i32'");
                    }
                    if (t2.kind != BaseTypeKind::Bool) {
                        emit_error(mc->arguments[2]->location, "HoldDownInterlock::update umbilical_connected must be 'bool'");
                    }
                }
                mc->resolved_callee = "HoldDownInterlock__update";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_thrust_terminated") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "HoldDownInterlock::is_thrust_terminated takes 0 arguments");
                }
                mc->resolved_callee = "HoldDownInterlock__is_thrust_terminated";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "abort_reason") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "HoldDownInterlock::abort_reason takes 0 arguments");
                }
                mc->resolved_callee = "HoldDownInterlock__abort_reason";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "clamp_strain") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "HoldDownInterlock::clamp_strain takes 0 arguments");
                }
                mc->resolved_callee = "HoldDownInterlock__clamp_strain";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "accel_z") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "HoldDownInterlock::accel_z takes 0 arguments");
                }
                mc->resolved_callee = "HoldDownInterlock__accel_z";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_nominal") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "HoldDownInterlock::is_nominal takes 0 arguments");
                }
                mc->resolved_callee = "HoldDownInterlock__is_nominal";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "reset_abort") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "HoldDownInterlock::reset_abort takes 0 arguments");
                }
                mc->resolved_callee = "HoldDownInterlock__reset_abort";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
        }

        if (base_struct == "TurbopumpMonitor") {
            if (mc->method_name == "update") {
                if (mc->arguments.size() != 3) {
                    emit_error(mc->location, "TurbopumpMonitor::update takes exactly 3 arguments (shaft_rpm, inlet_p, discharge_p)");
                } else {
                    for (size_t i = 0; i < 3; ++i) {
                        Type t = check_expression(*mc->arguments[i]);
                        if (t.kind != BaseTypeKind::I32) {
                            emit_error(mc->arguments[i]->location, "TurbopumpMonitor::update parameters must be 'i32'");
                        }
                    }
                }
                mc->resolved_callee = "TurbopumpMonitor__update";
                mc->resolved_self_kind = SelfKind::RefMut;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "state") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::state takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__state";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_nominal") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::is_nominal takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__is_nominal";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_warning") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::is_warning takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__is_warning";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "is_shutdown") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::is_shutdown takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__is_shutdown";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "trip_code") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::trip_code takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__trip_code";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "shaft_rpm") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::shaft_rpm takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__shaft_rpm";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "delta_p") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::delta_p takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__delta_p";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return mc->evaluated_type;
            }
            if (mc->method_name == "cavitation_count") {
                if (!mc->arguments.empty()) {
                    emit_error(mc->location, "TurbopumpMonitor::cavitation_count takes 0 arguments");
                }
                mc->resolved_callee = "TurbopumpMonitor__cavitation_count";
                mc->resolved_self_kind = SelfKind::Ref;
                mc->evaluated_type = Type{BaseTypeKind::I32, false, false};
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
