#pragma once

#include "ast.hpp"
#include "cfg.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace oxide {

struct Loan {
    size_t id = 0;
    std::string borrowed_var;
    bool is_mut = false;
    std::set<std::string> holders;
    SourceLocation location;
};

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

class BorrowChecker {
public:
    explicit BorrowChecker(Program& program);
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
