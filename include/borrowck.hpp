#pragma once

#include "ast.hpp"
#include "cfg.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace oxide {

/**
 * @brief Represents an active loan (reference) to an origin variable.
 *
 * In Oxide's Non-Lexical Lifetimes (NLL) system, a loan remains active
 * if and only if at least one reference holder variable is live according
 * to backward dataflow analysis.
 */
struct Loan {
    size_t id = 0;
    std::string borrowed_var;     ///< Target variable being loaned
    bool is_mut = false;          ///< True if this is an exclusive mutable borrow (&mut)
    std::set<std::string> holders; ///< Reference variable(s) holding this loan
    SourceLocation location;      ///< Source coordinate where loan was instantiated
};

/**
 * @brief Computes variable liveness across all basic blocks using backward dataflow analysis.
 *
 * Fixed-point iteration solves the standard dataflow equations:
 *   LiveOut[B] = Union_{S in Succ(B)} LiveIn[S]
 *   LiveIn[B]  = Uses(B) Union (LiveOut[B] \ Defs(B))
 */
class LivenessAnalyzer {
public:
    explicit LivenessAnalyzer(const CFGFunction& cfg);

    const std::set<std::string>& live_before(CFGPoint point) const;
    const std::set<std::string>& live_after(CFGPoint point) const;
    const std::set<std::string>& live_in(BlockId block) const;
    const std::set<std::string>& live_out(BlockId block) const;

private:
    void compute_liveness(const CFGFunction& cfg);

    std::map<BlockId, std::set<std::string>> live_in_;
    std::map<BlockId, std::set<std::string>> live_out_;
    std::map<CFGPoint, std::set<std::string>> live_before_;
    std::map<CFGPoint, std::set<std::string>> live_after_;
    std::set<std::string> empty_set_;
};

/**
 * @brief Enforces Rust's core borrow checker invariants and Non-Lexical Lifetimes (NLL).
 *
 * Rules verified:
 *  1. Mutability: Reassignment to immutable variables is statically rejected.
 *  2. Loan Exclusivity: Mutable borrow (&mut x) requires x to be mutable and cannot coexist
 *     with any other active borrow of x (mutable or immutable).
 *  3. Use While Borrowed: Origin variable cannot be modified or reborrowed mutably while active loans exist.
 *  4. Definite Initialization: Uses of potentially uninitialized variables are rejected.
 */
class BorrowChecker {
public:
    explicit BorrowChecker(Program& program);

    /**
     * @brief Runs borrow checking and NLL validation over all functions.
     * @return True if no borrow check violations exist; false otherwise.
     */
    bool check();

    const std::vector<std::string>& errors() const { return errors_; }

private:
    void check_function(Function& func);
    void check_function_nll(const CFGFunction& cfg, const LivenessAnalyzer& liveness);

    void emit_error(SourceLocation loc, const std::string& message);

    Program& program_;
    std::vector<std::string> errors_;
};

} // namespace oxide
