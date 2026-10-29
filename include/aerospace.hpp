#pragma once

#include "ast.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace oxide {

class AerospaceVerifier {
public:
    explicit AerospaceVerifier(const Program& program);
    bool verify();
    const std::vector<std::string>& errors() const { return errors_; }

private:
    void build_call_graph();
    void collect_calls_from_block(const std::string& caller, const BlockExpr& block);
    void collect_calls_from_expr(const std::string& caller, const Expression& expr);
    void check_recursion();

    void emit_error(SourceLocation loc, const std::string& message);

    const Program& program_;
    std::map<std::string, std::set<std::string>> call_graph_;
    std::map<std::string, SourceLocation> func_locations_;
    std::vector<std::string> errors_;
};

} // namespace oxide
