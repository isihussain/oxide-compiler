#pragma once

#include "token.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace oxide {

/**
 * @brief Discriminant indicating the fundamental category of an Oxide type.
 */
enum class BaseTypeKind {
    Unit,         ///< The zero-sized '()' type
    I32,          ///< Signed 32-bit integer
    Bool,         ///< Boolean value (true / false)
    Str,          ///< UTF-8 string slice (&str)
    Array,        ///< Fixed-size homogeneous array [T; N]
    Slice,        ///< Dynamically-sized slice view &[T]
    Named,        ///< Nominal user-defined struct or enum (e.g., Point, Status, Option)
    GenericParam, ///< Unbound type parameter during generic definition (e.g., T, E)
    PhysicalUnit, ///< SI physical unit dimension (e.g., Meter, Second, Kilogram, Newton, Pascal)
    F32,          ///< Single-precision 32-bit floating point (IEEE 754)
    F64,          ///< Double-precision 64-bit floating point (IEEE 754)
};

/**
 * @brief Exponent tuple (M^m * L^l * T^t) for compile-time dimensional analysis.
 *
 * M: Mass (kg), L: Length (meter), T: Time (second).
 */
struct PhysicalDimension {
    int8_t m = 0; ///< Mass exponent (Kilogram)
    int8_t l = 0; ///< Length exponent (Meter)
    int8_t t = 0; ///< Time exponent (Second)

    bool is_dimensionless() const { return m == 0 && l == 0 && t == 0; }
    bool operator==(const PhysicalDimension& other) const = default;

    std::string to_unit_name() const {
        if (m == 0 && l == 1 && t == 0) return "Meter";
        if (m == 0 && l == 0 && t == 1) return "Second";
        if (m == 1 && l == 0 && t == 0) return "Kilogram";
        if (m == 0 && l == 1 && t == -1) return "Mps";
        if (m == 0 && l == 1 && t == -2) return "Mps2";
        if (m == 1 && l == 1 && t == -2) return "Newton";
        if (m == 1 && l == -1 && t == -2) return "Pascal";
        if (m == 1 && l == 2 && t == -2) return "Joule";
        if (is_dimensionless()) return "i32";
        std::string s;
        if (m != 0) s += "Kg^" + std::to_string(m);
        if (l != 0) { if (!s.empty()) s += "*"; s += "m^" + std::to_string(l); }
        if (t != 0) { if (!s.empty()) s += "*"; s += "s^" + std::to_string(t); }
        return s;
    }
};

/**
 * @brief Canonical type representation in Oxide.
 *
 * Encapsulates the kind, nominal name, generic arguments, reference qualification,
 * mutability, array length, and physical unit dimension.
 */
struct Type {
    BaseTypeKind kind = BaseTypeKind::Unit;
    std::string name = "";               ///< Nominal type identifier or generic parameter name.
    std::vector<Type> type_args;        ///< Concrete type arguments for generic instantiation.
    bool is_ref = false;                 ///< True if this is an immutable reference (&T).
    bool is_mut_ref = false;             ///< True if this is a mutable reference (&mut T).
    size_t array_size = 0;               ///< Element count for BaseTypeKind::Array.
    PhysicalDimension dimension;         ///< SI exponents for BaseTypeKind::PhysicalUnit.

    Type() = default;
    Type(BaseTypeKind k, std::string n = "", bool r = false, bool mr = false, std::vector<Type> args = {}, size_t arr_sz = 0)
        : kind(k), name(std::move(n)), type_args(std::move(args)), is_ref(r), is_mut_ref(mr), array_size(arr_sz) {}
    Type(BaseTypeKind k, bool r, bool mr)
        : kind(k), name(""), is_ref(r), is_mut_ref(mr), array_size(0) {}
    Type(BaseTypeKind k, PhysicalDimension dim, std::string n = "")
        : kind(k), name(std::move(n)), dimension(dim) {}

    bool operator==(const Type& other) const {
        if (kind != other.kind) return false;
        if (is_ref != other.is_ref || is_mut_ref != other.is_mut_ref) return false;
        if (kind == BaseTypeKind::PhysicalUnit) {
            return dimension == other.dimension;
        }
        if (kind == BaseTypeKind::Array) {
            return array_size == other.array_size && type_args == other.type_args;
        }
        if (kind == BaseTypeKind::Slice) {
            return type_args == other.type_args;
        }
        return name == other.name && type_args == other.type_args;
    }

    /**
     * @brief Produces the idiomatic Rust string syntax (e.g. '&mut Point', '[i32; 4]', 'Option<i32>').
     */
    std::string to_string() const;

    /**
     * @brief Produces a sanitized symbol identifier for monomorphized C symbol generation.
     */
    std::string mangled_name() const;
};

// Forward declarations
struct Expression;
struct Statement;
struct BlockExpr;

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

enum class UnaryOp {
    Negate,
    Not,
    Ref,
    RefMut,
    Deref,
};

struct ASTNode {
    virtual ~ASTNode() = default;
    SourceLocation location;
};

struct Expression : public ASTNode {
    Type evaluated_type = Type{BaseTypeKind::Unit, false, false};
    virtual void dump(int indent = 0) const = 0;
};

struct IntegerLiteralExpr : public Expression {
    int64_t value = 0;
    explicit IntegerLiteralExpr(int64_t val, SourceLocation loc) : value(val) { location = loc; }
    void dump(int indent = 0) const override;
};

struct FloatLiteralExpr : public Expression {
    double value = 0.0;
    explicit FloatLiteralExpr(double val, SourceLocation loc) : value(val) { location = loc; }
    void dump(int indent = 0) const override;
};

struct BoolLiteralExpr : public Expression {
    bool value = false;
    explicit BoolLiteralExpr(bool val, SourceLocation loc) : value(val) { location = loc; }
    void dump(int indent = 0) const override;
};

struct VariableExpr : public Expression {
    std::string name;
    explicit VariableExpr(std::string var_name, SourceLocation loc) : name(std::move(var_name)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct BinaryExpr : public Expression {
    BinaryOp op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

    BinaryExpr(BinaryOp op, std::unique_ptr<Expression> l, std::unique_ptr<Expression> r, SourceLocation loc)
        : op(op), left(std::move(l)), right(std::move(r)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct UnaryExpr : public Expression {
    UnaryOp op;
    std::unique_ptr<Expression> operand;

    UnaryExpr(UnaryOp op, std::unique_ptr<Expression> expr, SourceLocation loc)
        : op(op), operand(std::move(expr)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct AssignExpr : public Expression {
    std::unique_ptr<Expression> target;
    std::unique_ptr<Expression> value;

    AssignExpr(std::unique_ptr<Expression> tgt, std::unique_ptr<Expression> val, SourceLocation loc)
        : target(std::move(tgt)), value(std::move(val)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct CallExpr : public Expression {
    std::string callee;
    std::vector<Type> type_args;
    std::vector<std::unique_ptr<Expression>> arguments;

    CallExpr(std::string callee, std::vector<std::unique_ptr<Expression>> args, SourceLocation loc,
             std::vector<Type> t_args = {})
        : callee(std::move(callee)), type_args(std::move(t_args)), arguments(std::move(args)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct IfExpr : public Expression {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockExpr> then_branch;
    std::unique_ptr<BlockExpr> else_branch; // Can be nullptr

    IfExpr(std::unique_ptr<Expression> cond, std::unique_ptr<BlockExpr> then_b,
           std::unique_ptr<BlockExpr> else_b, SourceLocation loc)
        : condition(std::move(cond)), then_branch(std::move(then_b)), else_branch(std::move(else_b)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct WhileExpr : public Expression {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockExpr> body;

    WhileExpr(std::unique_ptr<Expression> cond, std::unique_ptr<BlockExpr> b, SourceLocation loc)
        : condition(std::move(cond)), body(std::move(b)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct ReturnExpr : public Expression {
    std::unique_ptr<Expression> value; // Can be nullptr for unit return

    ReturnExpr(std::unique_ptr<Expression> val, SourceLocation loc)
        : value(std::move(val)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct StructFieldInit {
    std::string field_name;
    std::unique_ptr<Expression> value;
    SourceLocation location;
};

struct StructInitExpr : public Expression {
    std::string struct_name;
    std::vector<Type> type_args;
    std::vector<StructFieldInit> fields;

    StructInitExpr(std::string name, std::vector<StructFieldInit> flds, SourceLocation loc,
                   std::vector<Type> t_args = {})
        : struct_name(std::move(name)), type_args(std::move(t_args)), fields(std::move(flds)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct FieldAccessExpr : public Expression {
    std::unique_ptr<Expression> target;
    std::string field_name;

    FieldAccessExpr(std::unique_ptr<Expression> tgt, std::string field, SourceLocation loc)
        : target(std::move(tgt)), field_name(std::move(field)) { location = loc; }
    void dump(int indent = 0) const override;
};

enum class SelfKind {
    Value,
    Ref,
    RefMut,
};

struct MethodCallExpr : public Expression {
    std::unique_ptr<Expression> receiver;
    std::string method_name;
    std::vector<Type> type_args;
    std::vector<std::unique_ptr<Expression>> arguments;
    SelfKind resolved_self_kind = SelfKind::Value;
    std::string resolved_callee = "";

    MethodCallExpr(std::unique_ptr<Expression> recv, std::string method,
                   std::vector<std::unique_ptr<Expression>> args, SourceLocation loc,
                   std::vector<Type> t_args = {})
        : receiver(std::move(recv)), method_name(std::move(method)),
          type_args(std::move(t_args)), arguments(std::move(args)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct EnumVariantExpr : public Expression {
    std::string enum_name;
    std::string variant_name;
    std::unique_ptr<Expression> payload; // nullptr if unit variant

    EnumVariantExpr(std::string e_name, std::string v_name, std::unique_ptr<Expression> p, SourceLocation loc)
        : enum_name(std::move(e_name)), variant_name(std::move(v_name)), payload(std::move(p)) { location = loc; }
    void dump(int indent = 0) const override;
};

enum class PatternKind {
    Variant,
    Wildcard,
};

struct MatchPattern {
    PatternKind kind = PatternKind::Wildcard;
    std::string enum_name;
    std::string variant_name;
    std::string binding_name;
    SourceLocation location;
};

struct MatchArm {
    MatchPattern pattern;
    std::unique_ptr<Expression> body;
    SourceLocation location;
};

struct MatchExpr : public Expression {
    std::unique_ptr<Expression> target;
    std::vector<MatchArm> arms;

    MatchExpr(std::unique_ptr<Expression> tgt, std::vector<MatchArm> arm_list, SourceLocation loc)
        : target(std::move(tgt)), arms(std::move(arm_list)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct StringLiteralExpr : public Expression {
    std::string value;

    StringLiteralExpr(std::string val, SourceLocation loc)
        : value(std::move(val)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct ArrayLiteralExpr : public Expression {
    std::vector<std::unique_ptr<Expression>> elements;

    ArrayLiteralExpr(std::vector<std::unique_ptr<Expression>> elems, SourceLocation loc)
        : elements(std::move(elems)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct ArrayRepeatExpr : public Expression {
    std::unique_ptr<Expression> value;
    size_t count = 0;

    ArrayRepeatExpr(std::unique_ptr<Expression> val, size_t c, SourceLocation loc)
        : value(std::move(val)), count(c) { location = loc; }
    void dump(int indent = 0) const override;
};

struct IndexExpr : public Expression {
    std::unique_ptr<Expression> target;
    std::unique_ptr<Expression> index;

    IndexExpr(std::unique_ptr<Expression> tgt, std::unique_ptr<Expression> idx, SourceLocation loc)
        : target(std::move(tgt)), index(std::move(idx)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct RangeExpr : public Expression {
    std::unique_ptr<Expression> start;
    std::unique_ptr<Expression> end;

    RangeExpr(std::unique_ptr<Expression> s, std::unique_ptr<Expression> e, SourceLocation loc)
        : start(std::move(s)), end(std::move(e)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct ForExpr : public Expression {
    std::string var_name;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<BlockExpr> body;

    ForExpr(std::string var, std::unique_ptr<Expression> iter, std::unique_ptr<BlockExpr> b, SourceLocation loc)
        : var_name(std::move(var)), iterable(std::move(iter)), body(std::move(b)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct Statement : public ASTNode {
    virtual void dump(int indent = 0) const = 0;
};

struct LetStmt : public Statement {
    std::string var_name;
    bool is_mutable = false;
    Type declared_type;
    std::unique_ptr<Expression> initializer; // Can be nullptr
    std::vector<std::string> attributes;

    bool has_attribute(const std::string& attr) const {
        for (const auto& a : attributes) {
            if (a == attr) return true;
        }
        return false;
    }

    LetStmt(std::string name, bool is_mut, Type type, std::unique_ptr<Expression> init, SourceLocation loc)
        : var_name(std::move(name)), is_mutable(is_mut), declared_type(type), initializer(std::move(init)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct ExprStmt : public Statement {
    std::unique_ptr<Expression> expr;

    explicit ExprStmt(std::unique_ptr<Expression> e, SourceLocation loc)
        : expr(std::move(e)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct BlockExpr : public Expression {
    std::vector<std::unique_ptr<Statement>> statements;
    std::unique_ptr<Expression> trailing_expr; // Rust allows trailing expr as return value

    explicit BlockExpr(SourceLocation loc) { location = loc; }
    void dump(int indent = 0) const override;
};

struct Parameter {
    std::string name;
    Type type;
    bool is_mut = false;
    SourceLocation location;
};

struct Function : public ASTNode {
    std::string name;
    std::string abi = ""; // e.g. "C" or "" (Rust)
    std::string method_name = ""; // non-empty if part of an impl block
    std::string target_type = ""; // non-empty if part of an impl block
    std::vector<std::string> type_params;
    std::vector<Parameter> parameters;
    Type return_type;
    std::unique_ptr<BlockExpr> body;
    std::vector<std::string> attributes;

    bool has_attribute(const std::string& attr) const {
        for (const auto& a : attributes) {
            if (a == attr) return true;
        }
        return false;
    }

    Function(std::string name, std::vector<Parameter> params, Type ret_type,
             std::unique_ptr<BlockExpr> body, SourceLocation loc, std::string abi = "",
             std::vector<std::string> t_params = {})
        : name(std::move(name)), abi(std::move(abi)), type_params(std::move(t_params)),
          parameters(std::move(params)), return_type(ret_type),
          body(std::move(body)) { location = loc; }

    bool is_generic() const { return !type_params.empty(); }
    void dump(int indent = 0) const;
};

struct ForeignFunctionDecl {
    std::string name;
    std::vector<Parameter> parameters;
    Type return_type;
    SourceLocation location;
};

struct ExternBlock : public ASTNode {
    std::string abi; // e.g. "C"
    std::vector<ForeignFunctionDecl> declarations;

    ExternBlock(std::string abi, std::vector<ForeignFunctionDecl> decls, SourceLocation loc)
        : abi(std::move(abi)), declarations(std::move(decls)) { location = loc; }

    void dump(int indent = 0) const;
};

struct StructField {
    std::string name;
    Type type;
    SourceLocation location;
};

struct StructDef : public ASTNode {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<StructField> fields;

    StructDef(std::string name, std::vector<StructField> fields, SourceLocation loc,
              std::vector<std::string> t_params = {})
        : name(std::move(name)), type_params(std::move(t_params)), fields(std::move(fields)) { location = loc; }

    bool is_generic() const { return !type_params.empty(); }
    void dump(int indent = 0) const;
};

struct EnumVariant {
    std::string name;
    std::optional<Type> payload_type;
    int tag = 0;
    SourceLocation location;
};

struct EnumDef : public ASTNode {
    std::string name;
    std::vector<EnumVariant> variants;

    EnumDef(std::string name, std::vector<EnumVariant> vars, SourceLocation loc)
        : name(std::move(name)), variants(std::move(vars)) { location = loc; }

    void dump(int indent = 0) const;
};

struct TraitMethod {
    std::string name;
    std::vector<Parameter> parameters;
    Type return_type;
    SourceLocation location;
};

struct TraitDef : public ASTNode {
    std::string name;
    std::vector<TraitMethod> methods;

    TraitDef(std::string name, std::vector<TraitMethod> methods, SourceLocation loc)
        : name(std::move(name)), methods(std::move(methods)) { location = loc; }

    void dump(int indent = 0) const;
};

struct ImplBlock : public ASTNode {
    std::optional<std::string> trait_name; // e.g. "Shape"
    std::string target_type;              // e.g. "Rectangle"
    std::vector<std::unique_ptr<Function>> methods;

    ImplBlock(std::optional<std::string> trait, std::string target,
              std::vector<std::unique_ptr<Function>> methods, SourceLocation loc)
        : trait_name(std::move(trait)), target_type(std::move(target)),
          methods(std::move(methods)) { location = loc; }

    void dump(int indent = 0) const;
};

struct Program : public ASTNode {
    std::vector<std::unique_ptr<ExternBlock>> extern_blocks;
    std::vector<std::unique_ptr<StructDef>> structs;
    std::vector<std::unique_ptr<EnumDef>> enums;
    std::vector<std::unique_ptr<TraitDef>> traits;
    std::vector<std::unique_ptr<ImplBlock>> impl_blocks;
    std::vector<std::unique_ptr<Function>> functions;

    void dump() const;
};

} // namespace oxide
