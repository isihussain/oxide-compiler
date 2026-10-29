#pragma once

#include "token.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace oxide {

enum class BaseTypeKind {
    Unit,
    I32,
    Bool,
    Named,
};

struct Type {
    BaseTypeKind kind = BaseTypeKind::Unit;
    std::string name = "";
    bool is_ref = false;
    bool is_mut_ref = false;

    Type() = default;
    Type(BaseTypeKind k, std::string n = "", bool r = false, bool mr = false)
        : kind(k), name(std::move(n)), is_ref(r), is_mut_ref(mr) {}
    Type(BaseTypeKind k, bool r, bool mr)
        : kind(k), name(""), is_ref(r), is_mut_ref(mr) {}

    bool operator==(const Type& other) const = default;
    std::string to_string() const;
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
    std::vector<std::unique_ptr<Expression>> arguments;

    CallExpr(std::string callee, std::vector<std::unique_ptr<Expression>> args, SourceLocation loc)
        : callee(std::move(callee)), arguments(std::move(args)) { location = loc; }
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
    std::vector<StructFieldInit> fields;

    StructInitExpr(std::string name, std::vector<StructFieldInit> flds, SourceLocation loc)
        : struct_name(std::move(name)), fields(std::move(flds)) { location = loc; }
    void dump(int indent = 0) const override;
};

struct FieldAccessExpr : public Expression {
    std::unique_ptr<Expression> target;
    std::string field_name;

    FieldAccessExpr(std::unique_ptr<Expression> tgt, std::string field, SourceLocation loc)
        : target(std::move(tgt)), field_name(std::move(field)) { location = loc; }
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

struct Statement : public ASTNode {
    virtual void dump(int indent = 0) const = 0;
};

struct LetStmt : public Statement {
    std::string var_name;
    bool is_mutable = false;
    Type declared_type;
    std::unique_ptr<Expression> initializer; // Can be nullptr

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
    std::vector<Parameter> parameters;
    Type return_type;
    std::unique_ptr<BlockExpr> body;

    Function(std::string name, std::vector<Parameter> params, Type ret_type,
             std::unique_ptr<BlockExpr> body, SourceLocation loc, std::string abi = "")
        : name(std::move(name)), abi(std::move(abi)), parameters(std::move(params)), return_type(ret_type),
          body(std::move(body)) { location = loc; }

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
    std::vector<StructField> fields;

    StructDef(std::string name, std::vector<StructField> fields, SourceLocation loc)
        : name(std::move(name)), fields(std::move(fields)) { location = loc; }

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

struct Program : public ASTNode {
    std::vector<std::unique_ptr<ExternBlock>> extern_blocks;
    std::vector<std::unique_ptr<StructDef>> structs;
    std::vector<std::unique_ptr<EnumDef>> enums;
    std::vector<std::unique_ptr<Function>> functions;

    void dump() const;
};

} // namespace oxide
