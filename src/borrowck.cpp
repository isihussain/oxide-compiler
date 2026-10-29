#include "borrowck.hpp"
#include <iostream>
#include <sstream>

namespace oxide {

// --- LivenessAnalyzer Implementation ---

LivenessAnalyzer::LivenessAnalyzer(const CFGFunction& cfg) {
    compute_liveness(cfg);
}

const std::set<std::string>& LivenessAnalyzer::live_before(CFGPoint point) const {
    auto it = live_before_.find(point);
    if (it != live_before_.end()) return it->second;
    return empty_set_;
}

const std::set<std::string>& LivenessAnalyzer::live_after(CFGPoint point) const {
    auto it = live_after_.find(point);
    if (it != live_after_.end()) return it->second;
    return empty_set_;
}

const std::set<std::string>& LivenessAnalyzer::live_in(BlockId block) const {
    auto it = live_in_.find(block);
    if (it != live_in_.end()) return it->second;
    return empty_set_;
}

const std::set<std::string>& LivenessAnalyzer::live_out(BlockId block) const {
    auto it = live_out_.find(block);
    if (it != live_out_.end()) return it->second;
    return empty_set_;
}

void LivenessAnalyzer::compute_liveness(const CFGFunction& cfg) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = (int)cfg.blocks.size() - 1; i >= 0; --i) {
            const BasicBlock& bb = cfg.blocks[i];
            std::set<std::string> out;
            for (BlockId succ : bb.successors) {
                const auto& succ_in = live_in_[succ];
                out.insert(succ_in.begin(), succ_in.end());
            }
            live_out_[bb.id] = out;

            std::set<std::string> in = out;
            if (bb.terminator.kind == TerminatorKind::Branch && !bb.terminator.cond_var.empty()) {
                in.insert(bb.terminator.cond_var);
            } else if (bb.terminator.kind == TerminatorKind::Return && !bb.terminator.return_var.empty()) {
                in.insert(bb.terminator.return_var);
            }

            for (int s = (int)bb.statements.size() - 1; s >= 0; --s) {
                const CFGStatement& stmt = bb.statements[s];
                if (!stmt.target_var.empty() && stmt.kind == CFGStmtKind::AssignVar) {
                    in.erase(stmt.target_var);
                }
                for (const auto& r : stmt.read_vars) {
                    in.insert(r);
                }
            }

            if (in != live_in_[bb.id]) {
                live_in_[bb.id] = in;
                changed = true;
            }
        }
    }

    // Compute point-specific live_before and live_after
    for (const auto& bb : cfg.blocks) {
        std::set<std::string> cur_live = live_out_[bb.id];
        if (bb.terminator.kind == TerminatorKind::Branch && !bb.terminator.cond_var.empty()) {
            cur_live.insert(bb.terminator.cond_var);
        } else if (bb.terminator.kind == TerminatorKind::Return && !bb.terminator.return_var.empty()) {
            cur_live.insert(bb.terminator.return_var);
        }

        for (int s = (int)bb.statements.size() - 1; s >= 0; --s) {
            CFGPoint pt{bb.id, (size_t)s};
            live_after_[pt] = cur_live;

            const CFGStatement& stmt = bb.statements[s];
            if (!stmt.target_var.empty() && stmt.kind == CFGStmtKind::AssignVar) {
                cur_live.erase(stmt.target_var);
            }
            for (const auto& r : stmt.read_vars) {
                cur_live.insert(r);
            }
            live_before_[pt] = cur_live;
        }
    }
}

// --- BorrowChecker Implementation ---

BorrowChecker::BorrowChecker(Program& program)
    : program_(program) {}

void BorrowChecker::emit_error(SourceLocation loc, const std::string& message) {
    std::ostringstream ss;
    ss << "borrowck error[" << loc.line << ":" << loc.column << "]: " << message;
    errors_.push_back(ss.str());
}

bool BorrowChecker::check() {
    errors_.clear();

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

void BorrowChecker::check_function(Function& func) {
    CFGFunction cfg = CFGBuilder::build(func);
    LivenessAnalyzer liveness(cfg);
    check_function_nll(cfg, liveness);
}

void BorrowChecker::check_function_nll(const CFGFunction& cfg, const LivenessAnalyzer& liveness) {
    // 0. Compute Reachable Blocks
    std::set<BlockId> reachable_blocks;
    std::vector<BlockId> worklist = { cfg.entry_block };
    reachable_blocks.insert(cfg.entry_block);
    while (!worklist.empty()) {
        BlockId b = worklist.back();
        worklist.pop_back();
        if (b < cfg.blocks.size()) {
            for (BlockId succ : cfg.blocks[b].successors) {
                if (!reachable_blocks.count(succ)) {
                    reachable_blocks.insert(succ);
                    worklist.push_back(succ);
                }
            }
        }
    }

    // 1. Definite Initialization Analysis (Forward Dataflow - Must Analysis)
    std::map<BlockId, std::set<std::string>> init_in;
    std::map<BlockId, std::set<std::string>> init_out;

    std::set<std::string> param_names;
    for (const auto& p : cfg.parameters) {
        param_names.insert(p.name);
    }

    std::set<std::string> all_vars = param_names;
    for (const auto& kv : cfg.var_loc) {
        all_vars.insert(kv.first);
    }

    for (const auto& bb : cfg.blocks) {
        if (bb.id == cfg.entry_block) {
            init_in[bb.id] = param_names;
            init_out[bb.id] = param_names;
        } else {
            init_in[bb.id] = all_vars;
            init_out[bb.id] = all_vars;
        }
    }

    bool init_changed = true;
    while (init_changed) {
        init_changed = false;
        for (const auto& bb : cfg.blocks) {
            if (!reachable_blocks.count(bb.id)) continue;

            std::set<std::string> in_set;
            if (bb.id == cfg.entry_block) {
                in_set = param_names;
            } else {
                // Intersection of all reachable predecessors' init_out
                bool first = true;
                for (BlockId pred : bb.predecessors) {
                    if (!reachable_blocks.count(pred)) continue;
                    if (first) {
                        in_set = init_out[pred];
                        first = false;
                    } else {
                        std::set<std::string> inter;
                        const auto& pred_out = init_out[pred];
                        for (const auto& v : in_set) {
                            if (pred_out.count(v)) inter.insert(v);
                        }
                        in_set = inter;
                    }
                }
            }
            init_in[bb.id] = in_set;

            std::set<std::string> out_set = in_set;
            for (const auto& stmt : bb.statements) {
                if (!stmt.target_var.empty()) {
                    if (stmt.kind == CFGStmtKind::AssignVar || stmt.kind == CFGStmtKind::Borrow) {
                        out_set.insert(stmt.target_var);
                    }
                }
            }

            if (out_set != init_out[bb.id]) {
                init_out[bb.id] = out_set;
                init_changed = true;
            }
        }
    }

    // 2. Loan Tracking and Non-Lexical Conflict Detection
    std::vector<Loan> loans;
    size_t next_loan_id = 1;
    std::set<std::string> assigned_vars;
    // Parameters count as already initialized
    for (const auto& p : cfg.parameters) {
        assigned_vars.insert(p.name);
    }

    for (const auto& bb : cfg.blocks) {
        if (!reachable_blocks.count(bb.id)) continue;

        std::set<std::string> current_init = init_in[bb.id];

        for (size_t s = 0; s < bb.statements.size(); ++s) {
            CFGPoint pt{bb.id, s};
            const CFGStatement& stmt = bb.statements[s];
            const auto& live_after_pt = liveness.live_after(pt);

            // A) Check uninitialized variable reads
            for (const auto& r : stmt.read_vars) {
                if (cfg.var_loc.count(r) && !current_init.count(r)) {
                    emit_error(stmt.location, "use of possibly-uninitialized variable '" + r + "'");
                }
            }

            // B) Check assignment to immutable bindings
            if (stmt.kind == CFGStmtKind::AssignVar) {
                auto is_mut_it = cfg.var_is_mut.find(stmt.target_var);
                if (is_mut_it != cfg.var_is_mut.end() && !is_mut_it->second) {
                    if (assigned_vars.count(stmt.target_var)) {
                        emit_error(stmt.location, "cannot assign twice to immutable variable '" + stmt.target_var + "'");
                    }
                }
                assigned_vars.insert(stmt.target_var);
                current_init.insert(stmt.target_var);
            } else if (stmt.kind == CFGStmtKind::AssignField) {
                auto is_mut_it = cfg.var_is_mut.find(stmt.target_var);
                if (is_mut_it != cfg.var_is_mut.end() && !is_mut_it->second) {
                    emit_error(stmt.location, "cannot assign to field '" + stmt.field_name + "' of immutable binding '" + stmt.target_var + "'");
                }
            } else if (stmt.kind == CFGStmtKind::AssignDeref) {
                auto is_mut_ref_it = cfg.var_is_mut_ref.find(stmt.target_var);
                if (is_mut_ref_it != cfg.var_is_mut_ref.end() && !is_mut_ref_it->second) {
                    emit_error(stmt.location, "cannot assign to dereference of immutable reference");
                }
            }

            // C) Check mutation conflicts with active loans (Non-Lexical)
            if (stmt.kind == CFGStmtKind::AssignVar || stmt.kind == CFGStmtKind::AssignField) {
                for (const auto& loan : loans) {
                    if (loan.borrowed_var == stmt.target_var) {
                        bool loan_active = false;
                        for (const auto& h : loan.holders) {
                            if (live_after_pt.count(h)) {
                                loan_active = true;
                                break;
                            }
                        }
                        if (loan_active) {
                            emit_error(stmt.location, "cannot assign to '" + stmt.target_var + "' because it is borrowed");
                            break;
                        }
                    }
                }
            }

            // D) Check borrow statements (Non-Lexical)
            if (stmt.kind == CFGStmtKind::Borrow) {
                assigned_vars.insert(stmt.target_var);
                current_init.insert(stmt.target_var);

                if (stmt.is_mut_borrow) {
                    auto mut_it = cfg.var_is_mut.find(stmt.borrowed_var);
                    auto mut_ref_it = cfg.var_is_mut_ref.find(stmt.borrowed_var);
                    bool is_mut = (mut_it != cfg.var_is_mut.end() && mut_it->second);
                    bool is_mut_ref = (mut_ref_it != cfg.var_is_mut_ref.end() && mut_ref_it->second);
                    if (!is_mut && !is_mut_ref) {
                        emit_error(stmt.location, "cannot borrow immutable local variable '" + stmt.borrowed_var + "' as mutable");
                    }
                }

                // Check conflicts with existing active loans on stmt.borrowed_var
                for (const auto& loan : loans) {
                    if (loan.borrowed_var == stmt.borrowed_var) {
                        bool loan_active = false;
                        for (const auto& h : loan.holders) {
                            if (live_after_pt.count(h)) {
                                loan_active = true;
                                break;
                            }
                        }
                        if (loan_active) {
                            if (stmt.is_mut_borrow) {
                                if (loan.is_mut) {
                                    emit_error(stmt.location, "cannot borrow '" + stmt.borrowed_var + "' as mutable more than once at a time");
                                } else {
                                    emit_error(stmt.location, "cannot borrow '" + stmt.borrowed_var + "' as mutable because it is also borrowed as immutable");
                                }
                            } else {
                                if (loan.is_mut) {
                                    emit_error(stmt.location, "cannot borrow '" + stmt.borrowed_var + "' as immutable because it is also borrowed as mutable");
                                }
                            }
                            break;
                        }
                    }
                }

                // Register new loan
                loans.push_back(Loan{
                    next_loan_id++,
                    stmt.borrowed_var,
                    stmt.is_mut_borrow,
                    {stmt.target_var},
                    stmt.location
                });
            }

            // E) Check method call with &mut self borrow
            if (stmt.kind == CFGStmtKind::MethodCall) {
                if (stmt.is_mut_borrow && !stmt.borrowed_var.empty()) {
                    auto mut_it = cfg.var_is_mut.find(stmt.borrowed_var);
                    auto mut_ref_it = cfg.var_is_mut_ref.find(stmt.borrowed_var);
                    bool is_mut = (mut_it != cfg.var_is_mut.end() && mut_it->second);
                    bool is_mut_ref = (mut_ref_it != cfg.var_is_mut_ref.end() && mut_ref_it->second);
                    if (!is_mut && !is_mut_ref) {
                        emit_error(stmt.location, "cannot borrow immutable local variable '" + stmt.borrowed_var + "' as mutable in method call");
                    }

                    // Check active loans on receiver
                    for (const auto& loan : loans) {
                        if (loan.borrowed_var == stmt.borrowed_var) {
                            bool loan_active = false;
                            for (const auto& h : loan.holders) {
                                if (live_after_pt.count(h)) {
                                    loan_active = true;
                                    break;
                                }
                            }
                            if (loan_active) {
                                emit_error(stmt.location, "cannot borrow '" + stmt.borrowed_var + "' as mutable because it is also borrowed as immutable");
                                break;
                            }
                        }
                    }
                }
            }

            // F) Propagate loan holders on reference assignment: r2 = r1
            if (stmt.kind == CFGStmtKind::AssignVar && !stmt.target_var.empty()) {
                for (auto& loan : loans) {
                    for (const auto& r : stmt.read_vars) {
                        if (loan.holders.count(r)) {
                            loan.holders.insert(stmt.target_var);
                        }
                    }
                }
            }
        }
    }
}

} // namespace oxide
