#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mini {

struct SourceLocation {
    std::string file;
    int line = 1;
    int column = 1;

    SourceLocation() = default;
    SourceLocation(std::string fileName, int lineNumber, int columnNumber)
        : file(std::move(fileName)), line(lineNumber), column(columnNumber) {}
};

enum class TypeKind {
    Int,
    Bool,
    Float,
    Void,
    Error,
};

const char* toString(TypeKind type);

class Node {
public:
    explicit Node(SourceLocation location) : loc(std::move(location)) {}
    virtual ~Node() = default;

    SourceLocation loc;
};

class Expr : public Node {
public:
    explicit Expr(SourceLocation location) : Node(std::move(location)) {}
    ~Expr() override = default;

    TypeKind inferredType = TypeKind::Error;
};

using ExprPtr = std::unique_ptr<Expr>;

enum class UnaryOp {
    Negate,
    LogicalNot,
};

enum class BinaryOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LogicalAnd,
    LogicalOr,
};

const char* toString(UnaryOp op);
const char* toString(BinaryOp op);

class IntLiteralExpr final : public Expr {
public:
    IntLiteralExpr(SourceLocation location, std::int64_t literalValue)
        : Expr(std::move(location)), value(literalValue) {}

    std::int64_t value;
};

class VarExpr final : public Expr {
public:
    VarExpr(SourceLocation location, std::string variableName)
        : Expr(std::move(location)), name(std::move(variableName)) {}

    std::string name;
};

class UnaryExpr final : public Expr {
public:
    UnaryExpr(SourceLocation location, UnaryOp unaryOp, ExprPtr operandExpr)
        : Expr(std::move(location)), op(unaryOp), operand(std::move(operandExpr)) {}

    UnaryOp op;
    ExprPtr operand;
};

class BinaryExpr final : public Expr {
public:
    BinaryExpr(SourceLocation location, BinaryOp binaryOp, ExprPtr left, ExprPtr right)
        : Expr(std::move(location)), op(binaryOp), lhs(std::move(left)), rhs(std::move(right)) {}

    BinaryOp op;
    ExprPtr lhs;
    ExprPtr rhs;
};

class Stmt : public Node {
public:
    explicit Stmt(SourceLocation location) : Node(std::move(location)) {}
    ~Stmt() override = default;
};

using StmtPtr = std::unique_ptr<Stmt>;

class BlockStmt final : public Stmt {
public:
    explicit BlockStmt(SourceLocation location) : Stmt(std::move(location)) {}
    BlockStmt(SourceLocation location, std::vector<StmtPtr> blockStatements)
        : Stmt(std::move(location)), statements(std::move(blockStatements)) {}

    std::vector<StmtPtr> statements;
};

class VarDeclStmt final : public Stmt {
public:
    VarDeclStmt(SourceLocation location, TypeKind declaredTypeValue, std::string variableName, ExprPtr init)
        : Stmt(std::move(location)),
          declaredType(declaredTypeValue),
          name(std::move(variableName)),
          initializer(std::move(init)) {}

    TypeKind declaredType;
    std::string name;
    ExprPtr initializer;
};

class AssignStmt final : public Stmt {
public:
    AssignStmt(SourceLocation location, std::string variableName, ExprPtr assignedValue)
        : Stmt(std::move(location)), name(std::move(variableName)), value(std::move(assignedValue)) {}

    std::string name;
    ExprPtr value;
};

class ExprStmt final : public Stmt {
public:
    ExprStmt(SourceLocation location, ExprPtr expression)
        : Stmt(std::move(location)), expr(std::move(expression)) {}

    ExprPtr expr;
};

class IfStmt final : public Stmt {
public:
    IfStmt(SourceLocation location, ExprPtr cond, std::unique_ptr<BlockStmt> thenBranch,
           std::unique_ptr<BlockStmt> elseBranch)
        : Stmt(std::move(location)),
          condition(std::move(cond)),
          thenBlock(std::move(thenBranch)),
          elseBlock(std::move(elseBranch)) {}

    ExprPtr condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::unique_ptr<BlockStmt> elseBlock;
};

class ForStmt final : public Stmt {
public:
    ForStmt(SourceLocation location, StmtPtr initStmt, ExprPtr cond, StmtPtr stepStmt,
            std::unique_ptr<BlockStmt> loopBody)
        : Stmt(std::move(location)),
          init(std::move(initStmt)),
          condition(std::move(cond)),
          step(std::move(stepStmt)),
          body(std::move(loopBody)) {}

    StmtPtr init;
    ExprPtr condition;
    StmtPtr step;
    std::unique_ptr<BlockStmt> body;
};

class DoWhileStmt final : public Stmt {
public:
    DoWhileStmt(SourceLocation location, std::unique_ptr<BlockStmt> loopBody, ExprPtr cond)
        : Stmt(std::move(location)), body(std::move(loopBody)), condition(std::move(cond)) {}

    std::unique_ptr<BlockStmt> body;
    ExprPtr condition;
};

class ReturnStmt final : public Stmt {
public:
    ReturnStmt(SourceLocation location, ExprPtr returnValue)
        : Stmt(std::move(location)), value(std::move(returnValue)) {}

    ExprPtr value;
};

class BreakStmt final : public Stmt {
public:
    explicit BreakStmt(SourceLocation location) : Stmt(std::move(location)) {}
};

class ContinueStmt final : public Stmt {
public:
    explicit ContinueStmt(SourceLocation location) : Stmt(std::move(location)) {}
};

struct Param {
    SourceLocation loc;
    TypeKind type = TypeKind::Error;
    std::string name;

    Param() = default;
    Param(SourceLocation location, TypeKind paramType, std::string paramName)
        : loc(std::move(location)), type(paramType), name(std::move(paramName)) {}
};

class FunctionDecl final : public Node {
public:
    FunctionDecl(SourceLocation location, TypeKind returnTypeValue, std::string functionName,
                 std::vector<Param> functionParams, std::unique_ptr<BlockStmt> functionBody)
        : Node(std::move(location)),
          returnType(returnTypeValue),
          name(std::move(functionName)),
          params(std::move(functionParams)),
          body(std::move(functionBody)) {}

    TypeKind returnType;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
};

class Program final : public Node {
public:
    Program(SourceLocation location, std::vector<std::unique_ptr<FunctionDecl>> functionDecls)
        : Node(std::move(location)), functions(std::move(functionDecls)) {}

    std::vector<std::unique_ptr<FunctionDecl>> functions;
};

} // namespace mini
