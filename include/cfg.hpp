#pragma once

#include "ast.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace oxide {

using BlockId = size_t;

/**
 * @brief Categorization of fine-grained operations within a basic block.
 */
enum class CFGStmtKind {
    AssignVar,       // x = expr (variable assignment or rebinding)
    AssignField,     // x.field = expr (struct field assignment)
    AssignDeref,     // *ptr = expr (dereference assignment)
    AssignIndex,     // x[i] = expr (array/slice index assignment)
    Borrow,          // target = &borrowed or &mut borrowed (loan creation)
    MethodCall,      // r.method(...) (method invocation)
    Call,            // f(...) (function call)
    EvalExpr,        // standalone expression evaluation
};

/**
 * @brief Identifies a discrete program point (block + statement index) for liveness analysis.
 */
struct CFGPoint {
    BlockId block_id = 0;
    size_t stmt_index = 0;

    bool operator<(const CFGPoint& other) const {
        if (block_id != other.block_id) return block_id < other.block_id;
        return stmt_index < other.stmt_index;
    }
    bool operator==(const CFGPoint& other) const {
        return block_id == other.block_id && stmt_index == other.stmt_index;
    }
};

/**
 * @brief Atomic statement representation in the CFG tracking variable reads, writes, and loans.
 */
struct CFGStatement {
    CFGStmtKind kind = CFGStmtKind::EvalExpr;
    std::string target_var;
    std::string field_name;
    std::string borrowed_var;
    bool is_mut_borrow = false;
    std::vector<std::string> read_vars;
    SourceLocation location;
};

/**
 * @brief Control flow block terminator kinds.
 */
enum class TerminatorKind {
    None,   ///< Unfinalized block terminator
    Goto,   ///< Unconditional jump to target block
    Branch, ///< Two-way conditional branch based on boolean variable
    Return, ///< Exit from enclosing function
};

/**
 * @brief Block terminator transferring control to successor basic blocks.
 */
struct CFGTerminator {
    TerminatorKind kind = TerminatorKind::None;
    std::string cond_var;
    BlockId then_target = 0;
    BlockId else_target = 0;
    BlockId goto_target = 0;
    std::string return_var;
    SourceLocation location;
};

/**
 * @brief Basic block in a function's control flow graph.
 */
struct BasicBlock {
    BlockId id = 0;
    std::vector<CFGStatement> statements;
    CFGTerminator terminator;
    std::vector<BlockId> predecessors;
    std::vector<BlockId> successors;
};

/**
 * @brief Full Control Flow Graph for a single function.
 */
struct CFGFunction {
    std::string name;
    std::vector<Parameter> parameters;
    Type return_type;
    std::vector<BasicBlock> blocks;
    std::map<std::string, bool> var_is_mut;
    std::map<std::string, bool> var_is_ref;
    std::map<std::string, bool> var_is_mut_ref;
    std::map<std::string, SourceLocation> var_loc;
    BlockId entry_block = 0;

    BlockId new_block() {
        BlockId id = blocks.size();
        BasicBlock bb;
        bb.id = id;
        blocks.push_back(bb);
        return id;
    }

    void add_edge(BlockId from, BlockId to) {
        if (from >= blocks.size() || to >= blocks.size()) return;
        blocks[from].successors.push_back(to);
        blocks[to].predecessors.push_back(from);
    }
};

/**
 * @brief Lowers a high-level Function AST node into basic blocks and terminators.
 */
class CFGBuilder {
public:
    static CFGFunction build(const Function& func);

private:
    explicit CFGBuilder(const Function& func);
    void build_cfg(const Function& func);
    void lower_block(const BlockExpr& block, BlockId& current_bb);
    void lower_statement(const Statement& stmt, BlockId& current_bb);
    void lower_expression(const Expression& expr, BlockId& current_bb);
    void collect_read_vars(const Expression& expr, std::vector<std::string>& reads);

    CFGFunction cfg_;
};

} // namespace oxide
