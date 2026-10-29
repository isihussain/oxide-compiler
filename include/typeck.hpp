#pragma once

#include "ast.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace oxide {

/**
 * @brief Formal signature metadata for function declarations and FFI foreign functions.
 */
struct FunctionSignature {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Type> param_types;
    Type return_type;
    SourceLocation location;
};

/**
 * @brief Nominal struct layout record tracking field types and declared order.
 */
struct StructSignature {
    std::string name;
    std::vector<std::string> type_params;
    std::map<std::string, Type> fields;
    std::vector<std::string> field_order;
    SourceLocation location;
};

/**
 * @brief Individual variant within an algebraic tagged union enum.
 */
struct EnumVariantSignature {
    std::string name;
    std::optional<Type> payload_type;
    int tag = 0;
    SourceLocation location;
};

/**
 * @brief Definition metadata for tagged union enums and their discriminants.
 */
struct EnumSignature {
    std::string name;
    std::map<std::string, EnumVariantSignature> variants;
    SourceLocation location;
};

/**
 * @brief Abstract method declaration within a trait definition.
 */
struct TraitMethodSignature {
    std::string name;
    std::vector<Parameter> parameters;
    Type return_type;
    SourceLocation location;
};

/**
 * @brief Trait contract containing required method signatures.
 */
struct TraitSignature {
    std::string name;
    std::map<std::string, TraitMethodSignature> methods;
    SourceLocation location;
};

/**
 * @brief Resolved concrete method signature in an impl block.
 */
struct MethodSignature {
    std::string target_type;
    std::string method_name;
    std::string mangled_name;
    std::vector<Parameter> parameters;
    Type return_type;
    std::optional<std::string> trait_name;
    SourceLocation location;
};

/**
 * @brief Static type checker and semantic analyzer for Oxide.
 *
 * Responsibilities:
 *  - Enforces static type safety across expressions, statements, and blocks.
 *  - Manages a lexical scope stack for local variables with mutability tracking.
 *  - Registers and validates user-defined structs, enums, traits, and impl blocks.
 *  - Implements bidirectional type unification for polymorphic function and struct instantiation.
 *  - Enforces pattern matching exhaustiveness over enum variants.
 *  - Validates method dispatch receiver types and auto-deref/borrow rules.
 */
class TypeChecker {
public:
    explicit TypeChecker(Program& program);

    /**
     * @brief Performs full semantic type analysis over all items in the program.
     * @return True if no type errors were encountered; false otherwise.
     */
    bool check();

    const std::vector<std::string>& errors() const { return errors_; }

private:
    void register_traits();
    void register_structs();
    void register_enums();
    void register_functions();
    void register_impl_blocks();
    void check_function(Function& func);
    void check_block(BlockExpr& block, const Type& expected_return_type, bool is_function_root);
    void check_statement(Statement& stmt);
    Type check_expression(Expression& expr);

    Type substitute_type(const Type& type, const std::map<std::string, Type>& subst);
    bool unify_type(const Type& pattern, const Type& concrete, std::map<std::string, Type>& subst);

    void enter_scope();
    void exit_scope();
    void declare_variable(const std::string& name, const Type& type, bool is_mut, SourceLocation loc);
    std::optional<std::pair<Type, bool>> lookup_variable(const std::string& name);

    void emit_error(SourceLocation loc, const std::string& message);

    Program& program_;
    std::map<std::string, TraitSignature> trait_table_;
    std::map<std::pair<std::string, std::string>, MethodSignature> method_table_;
    std::map<std::string, StructSignature> struct_table_;
    std::map<std::string, EnumSignature> enum_table_;
    std::map<std::string, FunctionSignature> function_table_;
    std::vector<std::map<std::string, std::pair<Type, bool>>> scopes_; // name -> (Type, is_mut)
    std::optional<Type> current_function_return_type_;
    std::vector<std::string> errors_;
};

} // namespace oxide
