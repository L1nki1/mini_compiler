#include "ast.hpp"

namespace mini {

const char* toString(TypeKind type) {
    switch (type) {
    case TypeKind::Int:
        return "int";
    case TypeKind::Bool:
        return "bool";
    case TypeKind::Float:
        return "float";
    case TypeKind::Void:
        return "void";
    case TypeKind::Error:
        return "<error>";
    }
    return "<unknown>";
}

const char* toString(UnaryOp op) {
    switch (op) {
    case UnaryOp::Negate:
        return "-";
    case UnaryOp::LogicalNot:
        return "!";
    }
    return "<unknown unary op>";
}

const char* toString(BinaryOp op) {
    switch (op) {
    case BinaryOp::Add:
        return "+";
    case BinaryOp::Subtract:
        return "-";
    case BinaryOp::Multiply:
        return "*";
    case BinaryOp::Divide:
        return "/";
    case BinaryOp::Modulo:
        return "%";
    case BinaryOp::Equal:
        return "==";
    case BinaryOp::NotEqual:
        return "!=";
    case BinaryOp::Less:
        return "<";
    case BinaryOp::LessEqual:
        return "<=";
    case BinaryOp::Greater:
        return ">";
    case BinaryOp::GreaterEqual:
        return ">=";
    case BinaryOp::LogicalAnd:
        return "&&";
    case BinaryOp::LogicalOr:
        return "||";
    }
    return "<unknown binary op>";
}

} // namespace mini
