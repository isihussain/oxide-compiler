#pragma once

#include "ast.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace oxide {

/**
 * @brief Monomorphization transformation pass.
 *
 * Implements Rust's zero-cost abstraction paradigm for parametric polymorphism:
 *  1. Scans all function bodies, struct definitions, and call sites for generic instantiations.
 *  2. Clones generic AST templates (functions and structs) for each unique concrete type binding.
 *  3. Substitutes all GenericParam types with concrete types throughout parameters, return types,
 *     local bindings, and expression subtrees.
 *  4. Rewrites call sites and struct initializations to point directly to specialized mangled symbols.
 *  5. Prunes generic template definitions from the final AST before C99 code generation.
 */
class Monomorphizer {
public:
    explicit Monomorphizer(Program& program);
    void run();

private:
    std::string specialize_function(const std::string& orig_name, const std::vector<Type>& type_args);
    std::string specialize_struct(const std::string& orig_name, const std::vector<Type>& type_args);

    Type substitute_type(const Type& type, const std::map<std::string, Type>& subst);

    std::unique_ptr<Expression> clone_expr(const Expression& expr, const std::map<std::string, Type>& subst);
    std::unique_ptr<Statement> clone_stmt(const Statement& stmt, const std::map<std::string, Type>& subst);
    std::unique_ptr<BlockExpr> clone_block(const BlockExpr& block, const std::map<std::string, Type>& subst);

    void transform_expr(Expression& expr);
    void transform_stmt(Statement& stmt);
    void transform_block(BlockExpr& block);
    void transform_type(Type& type);

    Program& program_;
    std::map<std::string, const Function*> generic_functions_;
    std::map<std::string, const StructDef*> generic_structs_;

    std::map<std::string, std::unique_ptr<Function>> specialized_functions_;
    std::map<std::string, std::unique_ptr<StructDef>> specialized_structs_;
};

} // namespace oxide
