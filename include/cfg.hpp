#pragma once

#include "ast.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace oxide {

using BlockId = size_t;

enum class CFGStmtKind {
    AssignVar,       // x = expr
    AssignField,     // x.field = expr
    AssignDeref,     // *ptr = expr
    AssignIndex,     // x[i] = expr
    Borrow,          // target = &borrowed or &mut borrowed
    MethodCall,      // r.method(...)
    Call,            // f(...)
    EvalExpr,        // standalone expression
};

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

struct CFGStatement {
    CFGStmtKind kind = CFGStmtKind::EvalExpr;
    std::string target_var;
    std::string field_name;
    std::string borrowed_var;
    bool is_mut_borrow = false;
    std::vector<std::string> read_vars;
    SourceLocation location;
};

enum class TerminatorKind {
    None,
    Goto,
    Branch,
    Return,
};

struct CFGTerminator {
    TerminatorKind kind = TerminatorKind::None;
    std::string cond_var;
    BlockId then_target = 0;
    BlockId else_target = 0;
    BlockId goto_target = 0;
    std::string return_var;
    SourceLocation location;
};

struct BasicBlock {
    BlockId id = 0;
    std::vector<CFGStatement> statements;
    CFGTerminator terminator;
    std::vector<BlockId> predecessors;
    std::vector<BlockId> successors;
};

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
