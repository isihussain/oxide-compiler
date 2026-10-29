#include "ast.hpp"
#include <iostream>

namespace oxide {

std::string Type::to_string() const {
    std::string s;
    if (is_ref) {
        s += "&";
        if (is_mut_ref) {
            s += "mut ";
        }
    }
    switch (kind) {
        case BaseTypeKind::Unit: s += "()"; break;
        case BaseTypeKind::I32: s += "i32"; break;
        case BaseTypeKind::Bool: s += "bool"; break;
        case BaseTypeKind::Named: {
            s += name;
            if (!type_args.empty()) {
                s += "<";
                for (size_t i = 0; i < type_args.size(); ++i) {
                    if (i > 0) s += ", ";
                    s += type_args[i].to_string();
                }
                s += ">";
            }
            break;
        }
        case BaseTypeKind::GenericParam: s += name; break;
    }
    return s;
}

std::string Type::mangled_name() const {
    std::string s;
    switch (kind) {
        case BaseTypeKind::Unit: s = "unit"; break;
        case BaseTypeKind::I32: s = "i32"; break;
        case BaseTypeKind::Bool: s = "bool"; break;
        case BaseTypeKind::Named: {
            s = name;
            for (const auto& arg : type_args) {
                s += "_" + arg.mangled_name();
            }
            break;
        }
        case BaseTypeKind::GenericParam: s = name; break;
    }
    return s;
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; ++i) {
        std::cout << "  ";
    }
}

void IntegerLiteralExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "IntLiteral(" << value << ") : " << evaluated_type.to_string() << "\n";
}

void BoolLiteralExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "BoolLiteral(" << (value ? "true" : "false") << ") : " << evaluated_type.to_string() << "\n";
}

void VariableExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "Var(" << name << ") : " << evaluated_type.to_string() << "\n";
}

void BinaryExpr::dump(int indent) const {
    print_indent(indent);
    const char* op_str = "?";
    switch (op) {
        case BinaryOp::Add: op_str = "+"; break;
        case BinaryOp::Sub: op_str = "-"; break;
        case BinaryOp::Mul: op_str = "*"; break;
        case BinaryOp::Div: op_str = "/"; break;
        case BinaryOp::Equal: op_str = "=="; break;
        case BinaryOp::NotEqual: op_str = "!="; break;
        case BinaryOp::Less: op_str = "<"; break;
        case BinaryOp::LessEqual: op_str = "<="; break;
        case BinaryOp::Greater: op_str = ">"; break;
        case BinaryOp::GreaterEqual: op_str = ">="; break;
    }
    std::cout << "BinaryExpr(" << op_str << ") : " << evaluated_type.to_string() << "\n";
    if (left) left->dump(indent + 1);
    if (right) right->dump(indent + 1);
}

void UnaryExpr::dump(int indent) const {
    print_indent(indent);
    const char* op_str = "?";
    switch (op) {
        case UnaryOp::Negate: op_str = "-"; break;
        case UnaryOp::Not: op_str = "!"; break;
        case UnaryOp::Ref: op_str = "&"; break;
        case UnaryOp::RefMut: op_str = "&mut "; break;
        case UnaryOp::Deref: op_str = "*"; break;
    }
    std::cout << "UnaryExpr(" << op_str << ") : " << evaluated_type.to_string() << "\n";
    if (operand) operand->dump(indent + 1);
}

void AssignExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "AssignExpr : " << evaluated_type.to_string() << "\n";
    print_indent(indent + 1);
    std::cout << "Target:\n";
    if (target) target->dump(indent + 2);
    print_indent(indent + 1);
    std::cout << "Value:\n";
    if (value) value->dump(indent + 2);
}

void CallExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "CallExpr(" << callee << ") : " << evaluated_type.to_string() << "\n";
    for (const auto& arg : arguments) {
        arg->dump(indent + 1);
    }
}

void IfExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "IfExpr : " << evaluated_type.to_string() << "\n";
    print_indent(indent + 1);
    std::cout << "Condition:\n";
    if (condition) condition->dump(indent + 2);
    print_indent(indent + 1);
    std::cout << "Then:\n";
    if (then_branch) then_branch->dump(indent + 2);
    if (else_branch) {
        print_indent(indent + 1);
        std::cout << "Else:\n";
        else_branch->dump(indent + 2);
    }
}

void WhileExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "WhileExpr : " << evaluated_type.to_string() << "\n";
    print_indent(indent + 1);
    std::cout << "Condition:\n";
    if (condition) condition->dump(indent + 2);
    print_indent(indent + 1);
    std::cout << "Body:\n";
    if (body) body->dump(indent + 2);
}

void ReturnExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "ReturnExpr : " << evaluated_type.to_string() << "\n";
    if (value) value->dump(indent + 1);
}

void StructInitExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "StructInit(" << struct_name << ") : " << evaluated_type.to_string() << " {\n";
    for (const auto& field : fields) {
        print_indent(indent + 1);
        std::cout << field.field_name << ":\n";
        if (field.value) field.value->dump(indent + 2);
    }
    print_indent(indent);
    std::cout << "}\n";
}

void FieldAccessExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "FieldAccess(." << field_name << ") : " << evaluated_type.to_string() << "\n";
    if (target) target->dump(indent + 1);
}

void MethodCallExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "MethodCall(." << method_name << ") : " << evaluated_type.to_string() << "\n";
    print_indent(indent + 1);
    std::cout << "Receiver:\n";
    if (receiver) receiver->dump(indent + 2);
    if (!arguments.empty()) {
        print_indent(indent + 1);
        std::cout << "Arguments:\n";
        for (const auto& arg : arguments) {
            arg->dump(indent + 2);
        }
    }
}

void EnumVariantExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "EnumVariantExpr(" << enum_name << "::" << variant_name << ") : " << evaluated_type.to_string() << "\n";
    if (payload) {
        payload->dump(indent + 1);
    }
}

void MatchExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "MatchExpr : " << evaluated_type.to_string() << "\n";
    print_indent(indent + 1);
    std::cout << "Target:\n";
    if (target) target->dump(indent + 2);
    print_indent(indent + 1);
    std::cout << "Arms:\n";
    for (const auto& arm : arms) {
        print_indent(indent + 2);
        if (arm.pattern.kind == PatternKind::Wildcard) {
            std::cout << "_ =>\n";
        } else {
            std::cout << arm.pattern.enum_name << "::" << arm.pattern.variant_name;
            if (!arm.pattern.binding_name.empty()) {
                std::cout << "(" << arm.pattern.binding_name << ")";
            }
            std::cout << " =>\n";
        }
        if (arm.body) arm.body->dump(indent + 3);
    }
}

void LetStmt::dump(int indent) const {
    print_indent(indent);
    std::cout << "LetStmt(" << (is_mutable ? "mut " : "") << var_name << " : "
              << declared_type.to_string() << ")\n";
    if (initializer) {
        initializer->dump(indent + 1);
    }
}

void ExprStmt::dump(int indent) const {
    print_indent(indent);
    std::cout << "ExprStmt:\n";
    if (expr) expr->dump(indent + 1);
}

void BlockExpr::dump(int indent) const {
    print_indent(indent);
    std::cout << "BlockExpr : " << evaluated_type.to_string() << " {\n";
    for (const auto& stmt : statements) {
        stmt->dump(indent + 1);
    }
    if (trailing_expr) {
        print_indent(indent + 1);
        std::cout << "TrailingExpr:\n";
        trailing_expr->dump(indent + 2);
    }
    print_indent(indent);
    std::cout << "}\n";
}

void StructDef::dump(int indent) const {
    print_indent(indent);
    std::cout << "struct " << name << " {\n";
    for (const auto& f : fields) {
        print_indent(indent + 1);
        std::cout << f.name << ": " << f.type.to_string() << ",\n";
    }
    print_indent(indent);
    std::cout << "}\n";
}

void ExternBlock::dump(int indent) const {
    print_indent(indent);
    std::cout << "extern \"" << abi << "\" {\n";
    for (const auto& decl : declarations) {
        print_indent(indent + 1);
        std::cout << "fn " << decl.name << "(";
        for (size_t i = 0; i < decl.parameters.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << decl.parameters[i].name << ": " << decl.parameters[i].type.to_string();
        }
        std::cout << ") -> " << decl.return_type.to_string() << ";\n";
    }
    print_indent(indent);
    std::cout << "}\n";
}

void Function::dump(int indent) const {
    print_indent(indent);
    if (!abi.empty()) {
        std::cout << "extern \"" << abi << "\" ";
    }
    std::cout << "Function " << name << "(";
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << (parameters[i].is_mut ? "mut " : "") << parameters[i].name
                  << ": " << parameters[i].type.to_string();
    }
    std::cout << ") -> " << return_type.to_string() << "\n";
    if (body) {
        body->dump(indent + 1);
    }
}

void EnumDef::dump(int indent) const {
    print_indent(indent);
    std::cout << "Enum " << name << " {\n";
    for (const auto& var : variants) {
        print_indent(indent + 1);
        std::cout << var.name;
        if (var.payload_type.has_value()) {
            std::cout << "(" << var.payload_type->to_string() << ")";
        }
        std::cout << " = " << var.tag << ",\n";
    }
    print_indent(indent);
    std::cout << "}\n";
}

void TraitDef::dump(int indent) const {
    print_indent(indent);
    std::cout << "Trait " << name << " {\n";
    for (const auto& m : methods) {
        print_indent(indent + 1);
        std::cout << "fn " << m.name << "(";
        for (size_t i = 0; i < m.parameters.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << (m.parameters[i].is_mut ? "mut " : "") << m.parameters[i].name
                      << ": " << m.parameters[i].type.to_string();
        }
        std::cout << ") -> " << m.return_type.to_string() << ";\n";
    }
    print_indent(indent);
    std::cout << "}\n";
}

void ImplBlock::dump(int indent) const {
    print_indent(indent);
    std::cout << "Impl ";
    if (trait_name.has_value()) {
        std::cout << *trait_name << " for ";
    }
    std::cout << target_type << " {\n";
    for (const auto& fn : methods) {
        fn->dump(indent + 1);
    }
    print_indent(indent);
    std::cout << "}\n";
}

void Program::dump() const {
    std::cout << "Program {\n";
    for (const auto& eb : extern_blocks) {
        eb->dump(1);
    }
    for (const auto& s : structs) {
        s->dump(1);
    }
    for (const auto& e : enums) {
        e->dump(1);
    }
    for (const auto& t : traits) {
        t->dump(1);
    }
    for (const auto& ib : impl_blocks) {
        ib->dump(1);
    }
    for (const auto& fn : functions) {
        fn->dump(1);
    }
    std::cout << "}\n";
}

} // namespace oxide
