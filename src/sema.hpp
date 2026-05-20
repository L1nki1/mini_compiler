#pragma once

#include "ast.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace mini {

class SemanticAnalyzer {
public:
    bool analyze(Program& program);

    const std::vector<std::string>& errors() const;
    bool hasErrors() const;

private:
    void checkFunction(FunctionDecl& function);
    void checkBlock(BlockStmt& block, bool createScope);
    void checkStmt(Stmt& stmt);
    TypeKind checkExpr(Expr& expr);

    void checkVarDecl(VarDeclStmt& stmt);
    void checkAssign(AssignStmt& stmt);
    void checkIf(IfStmt& stmt);
    void checkFor(ForStmt& stmt);
    void checkDoWhile(DoWhileStmt& stmt);
    void checkReturn(ReturnStmt& stmt);
    void checkBreak(BreakStmt& stmt);
    void checkContinue(ContinueStmt& stmt);

    bool stmtAlwaysReturns(const Stmt& stmt) const;
    bool blockAlwaysReturns(const BlockStmt& block) const;

    void enterScope();
    void leaveScope();
    bool declareSymbol(const SourceLocation& loc, const std::string& name, TypeKind type);
    TypeKind lookupSymbol(const std::string& name) const;

    bool ensureImplementedType(const SourceLocation& loc, TypeKind type);
    void requireType(const SourceLocation& loc, TypeKind actual, TypeKind expected, const std::string& what);
    void error(const SourceLocation& loc, const std::string& message);
    std::string formatLocation(const SourceLocation& loc) const;

    std::vector<std::unordered_map<std::string, TypeKind>> scopes_;
    std::vector<std::string> errors_;
    TypeKind currentReturnType_ = TypeKind::Error;
    std::string currentFunctionName_;
    int loopDepth_ = 0;
};

} // namespace mini
