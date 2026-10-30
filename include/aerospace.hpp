#pragma once

#include "ast.hpp"
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace oxide {

/**
 * @brief Safety-critical flight software verification engine for Oxide.
 *
 * Activated under the `--profile=aerospace` flag. Enforces constraints inspired
 * by NASA/JPL "Ten Rules for Developing Safety-Critical Code" and aerospace standards:
 *
 *  - NASA/JPL Rule 1: Static loop boundedness analysis (unbounded 'while true' loops are rejected).
 *  - NASA/JPL Rule 2: Dynamic memory allocation ban (calls to 'malloc', 'calloc', 'realloc', 'free' are rejected).
 *  - NASA/JPL Rule 3: Bounded call stack depth (direct and mutual recursion rejected via call graph cycle detection).
 *  - Ariane 5 Mitigation: Checked arithmetic with hardware/compiler overflow traps (__builtin_*_overflow).
 *  - Bounds Safety: Array access bounds validation both statically and via runtime trap emissions.
 */
class AerospaceVerifier {
public:
    explicit AerospaceVerifier(const Program& program);

    /**
     * @brief Performs full safety-critical verification across the program.
     * @return True if no safety-critical rules were violated; false otherwise.
     */
    bool verify();

    const std::vector<std::string>& errors() const { return errors_; }

private:
    void build_call_graph();
    void collect_calls_from_block(const std::string& caller, const BlockExpr& block);
    void collect_calls_from_expr(const std::string& caller, const Expression& expr);
    void check_recursion();
    void check_loop_bounds();
    void check_memory_allocation();
    void check_volatile_barriers();
    void check_block_barriers(const BlockExpr& block, std::optional<SourceLocation>& last_write_loc);

    void emit_error(SourceLocation loc, const std::string& message);

    const Program& program_;
    std::map<std::string, std::set<std::string>> call_graph_;
    std::map<std::string, SourceLocation> func_locations_;
    std::vector<std::string> errors_;
};

} // namespace oxide
