#include "sema.hpp"

#include <sstream>
#include <unordered_set>

namespace mini {

bool SemanticAnalyzer::analyze(Program& program) {
    errors_.clear();
    scopes_.clear();
    currentReturnType_ = TypeKind::Error;
    currentFunctionName_.clear();
    loopDepth_ = 0;

    bool hasCompiledFn = false;
    std::unordered_set<std::string> functionNames;

    for (const auto& function : program.functions) {
        if (!functionNames.insert(function->name).second) {
            error(function->loc, "function '" + function->name + "' is already defined");
        }
        if (function->name == "compiled_fn") {
            hasCompiledFn = true;
        }
    }

    if (!hasCompiledFn) {
        error(program.loc, "required function 'compiled_fn' is missing");
    }

    for (const auto& function : program.functions) {
        checkFunction(*function);
    }

    return !hasErrors();
}

const std::vector<std::string>& SemanticAnalyzer::errors() const {
    return errors_;
}

bool SemanticAnalyzer::hasErrors() const {
    return !errors_.empty();
}

void SemanticAnalyzer::checkFunction(FunctionDecl& function) {
    currentReturnType_ = function.returnType;
    currentFunctionName_ = function.name;

    if (!ensureImplementedType(function.loc, function.returnType)) {
        currentReturnType_ = TypeKind::Error;
    }

    if (function.name == "compiled_fn") {
        if (function.returnType != TypeKind::Int) {
            error(function.loc, "compiled_fn must return int");
        }
        if (function.params.size() != 1) {
            error(function.loc, "compiled_fn must have exactly one int parameter");
        } else if (function.params.front().type != TypeKind::Int) {
            error(function.params.front().loc, "compiled_fn parameter must have type int");
        }
    }

    enterScope();
    for (const Param& param : function.params) {
        TypeKind paramType = param.type;
        if (!ensureImplementedType(param.loc, param.type)) {
            paramType = TypeKind::Error;
        }
        declareSymbol(param.loc, param.name, paramType);
    }

    checkBlock(*function.body, false);

    if (currentReturnType_ != TypeKind::Void && currentReturnType_ != TypeKind::Error &&
        !blockAlwaysReturns(*function.body)) {
        error(function.loc, "function '" + function.name + "' may not return a value on all paths");
    }

    leaveScope();
    currentReturnType_ = TypeKind::Error;
    currentFunctionName_.clear();
}

void SemanticAnalyzer::checkBlock(BlockStmt& block, bool createScope) {
    if (createScope) {
        enterScope();
    }

    for (const auto& statement : block.statements) {
        checkStmt(*statement);
    }

    if (createScope) {
        leaveScope();
    }
}

void SemanticAnalyzer::checkStmt(Stmt& stmt) {
    if (auto* block = dynamic_cast<BlockStmt*>(&stmt)) {
        checkBlock(*block, true);
    } else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(&stmt)) {
        checkVarDecl(*varDecl);
    } else if (auto* assign = dynamic_cast<AssignStmt*>(&stmt)) {
        checkAssign(*assign);
    } else if (auto* ifStmt = dynamic_cast<IfStmt*>(&stmt)) {
        checkIf(*ifStmt);
    } else if (auto* forStmt = dynamic_cast<ForStmt*>(&stmt)) {
        checkFor(*forStmt);
    } else if (auto* doWhileStmt = dynamic_cast<DoWhileStmt*>(&stmt)) {
        checkDoWhile(*doWhileStmt);
    } else if (auto* returnStmt = dynamic_cast<ReturnStmt*>(&stmt)) {
        checkReturn(*returnStmt);
    } else if (auto* breakStmt = dynamic_cast<BreakStmt*>(&stmt)) {
        checkBreak(*breakStmt);
    } else if (auto* continueStmt = dynamic_cast<ContinueStmt*>(&stmt)) {
        checkContinue(*continueStmt);
    } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(&stmt)) {
        checkExpr(*exprStmt->expr);
    }
}

TypeKind SemanticAnalyzer::checkExpr(Expr& expr) {
    if (auto* literal = dynamic_cast<IntLiteralExpr*>(&expr)) {
        (void)literal;
        expr.inferredType = TypeKind::Int;
        return expr.inferredType;
    }

    if (auto* var = dynamic_cast<VarExpr*>(&expr)) {
        const TypeKind type = lookupSymbol(var->name);
        if (type == TypeKind::Error) {
            error(var->loc, "variable '" + var->name + "' is not declared");
        }
        expr.inferredType = type;
        return expr.inferredType;
    }

    if (auto* unary = dynamic_cast<UnaryExpr*>(&expr)) {
        const TypeKind operandType = checkExpr(*unary->operand);
        switch (unary->op) {
        case UnaryOp::Negate:
            requireType(unary->loc, operandType, TypeKind::Int, "unary '-' operand");
            expr.inferredType = operandType == TypeKind::Int ? TypeKind::Int : TypeKind::Error;
            return expr.inferredType;
        case UnaryOp::LogicalNot:
            requireType(unary->loc, operandType, TypeKind::Bool, "unary '!' operand");
            expr.inferredType = operandType == TypeKind::Bool ? TypeKind::Bool : TypeKind::Error;
            return expr.inferredType;
        }
    }

    if (auto* binary = dynamic_cast<BinaryExpr*>(&expr)) {
        const TypeKind lhsType = checkExpr(*binary->lhs);
        const TypeKind rhsType = checkExpr(*binary->rhs);

        switch (binary->op) {
        case BinaryOp::Add:
        case BinaryOp::Subtract:
        case BinaryOp::Multiply:
        case BinaryOp::Divide:
        case BinaryOp::Modulo:
            requireType(binary->lhs->loc, lhsType, TypeKind::Int,
                        std::string("left operand of '") + toString(binary->op) + "'");
            requireType(binary->rhs->loc, rhsType, TypeKind::Int,
                        std::string("right operand of '") + toString(binary->op) + "'");
            expr.inferredType = lhsType == TypeKind::Int && rhsType == TypeKind::Int ? TypeKind::Int : TypeKind::Error;
            return expr.inferredType;

        case BinaryOp::Less:
        case BinaryOp::LessEqual:
        case BinaryOp::Greater:
        case BinaryOp::GreaterEqual:
            requireType(binary->lhs->loc, lhsType, TypeKind::Int,
                        std::string("left operand of '") + toString(binary->op) + "'");
            requireType(binary->rhs->loc, rhsType, TypeKind::Int,
                        std::string("right operand of '") + toString(binary->op) + "'");
            expr.inferredType = lhsType == TypeKind::Int && rhsType == TypeKind::Int ? TypeKind::Bool : TypeKind::Error;
            return expr.inferredType;

        case BinaryOp::Equal:
        case BinaryOp::NotEqual:
            if (lhsType != TypeKind::Error && rhsType != TypeKind::Error && lhsType != rhsType) {
                error(binary->loc, std::string("operator '") + toString(binary->op) +
                                       "' requires operands of the same type");
            }
            expr.inferredType =
                lhsType != TypeKind::Error && rhsType != TypeKind::Error && lhsType == rhsType ? TypeKind::Bool
                                                                                                : TypeKind::Error;
            return expr.inferredType;

        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            requireType(binary->lhs->loc, lhsType, TypeKind::Bool,
                        std::string("left operand of '") + toString(binary->op) + "'");
            requireType(binary->rhs->loc, rhsType, TypeKind::Bool,
                        std::string("right operand of '") + toString(binary->op) + "'");
            expr.inferredType = lhsType == TypeKind::Bool && rhsType == TypeKind::Bool ? TypeKind::Bool
                                                                                       : TypeKind::Error;
            return expr.inferredType;
        }
    }

    expr.inferredType = TypeKind::Error;
    return expr.inferredType;
}

void SemanticAnalyzer::checkVarDecl(VarDeclStmt& stmt) {
    TypeKind declaredType = stmt.declaredType;
    if (!ensureImplementedType(stmt.loc, declaredType)) {
        declaredType = TypeKind::Error;
    }

    declareSymbol(stmt.loc, stmt.name, declaredType);

    if (stmt.initializer) {
        const TypeKind initializerType = checkExpr(*stmt.initializer);
        if (declaredType != TypeKind::Error && initializerType != TypeKind::Error && declaredType != initializerType) {
            error(stmt.initializer->loc, "initializer for variable '" + stmt.name + "' has type " +
                                             toString(initializerType) + ", expected " + toString(declaredType));
        }
    }
}

void SemanticAnalyzer::checkAssign(AssignStmt& stmt) {
    const TypeKind variableType = lookupSymbol(stmt.name);
    if (variableType == TypeKind::Error) {
        error(stmt.loc, "variable '" + stmt.name + "' is not declared");
    }

    const TypeKind valueType = checkExpr(*stmt.value);
    if (variableType != TypeKind::Error && valueType != TypeKind::Error && variableType != valueType) {
        error(stmt.value->loc, "cannot assign " + std::string(toString(valueType)) + " to variable '" +
                                  stmt.name + "' of type " + toString(variableType));
    }
}

void SemanticAnalyzer::checkIf(IfStmt& stmt) {
    const TypeKind conditionType = checkExpr(*stmt.condition);
    requireType(stmt.condition->loc, conditionType, TypeKind::Bool, "if condition");
    checkBlock(*stmt.thenBlock, true);
    if (stmt.elseBlock) {
        checkBlock(*stmt.elseBlock, true);
    }
}

void SemanticAnalyzer::checkFor(ForStmt& stmt) {
    enterScope();
    if (stmt.init) {
        checkStmt(*stmt.init);
    }

    if (stmt.condition) {
        const TypeKind conditionType = checkExpr(*stmt.condition);
        requireType(stmt.condition->loc, conditionType, TypeKind::Bool, "for condition");
    }

    ++loopDepth_;
    checkBlock(*stmt.body, true);
    --loopDepth_;

    if (stmt.step) {
        checkStmt(*stmt.step);
    }
    leaveScope();
}

void SemanticAnalyzer::checkDoWhile(DoWhileStmt& stmt) {
    ++loopDepth_;
    checkBlock(*stmt.body, true);
    --loopDepth_;

    const TypeKind conditionType = checkExpr(*stmt.condition);
    requireType(stmt.condition->loc, conditionType, TypeKind::Bool, "do/while condition");
}

void SemanticAnalyzer::checkReturn(ReturnStmt& stmt) {
    const TypeKind valueType = checkExpr(*stmt.value);
    if (currentReturnType_ != TypeKind::Error && valueType != TypeKind::Error && currentReturnType_ != valueType) {
        error(stmt.value->loc, "return expression has type " + std::string(toString(valueType)) + ", expected " +
                                  toString(currentReturnType_));
    }
}

void SemanticAnalyzer::checkBreak(BreakStmt& stmt) {
    if (loopDepth_ == 0) {
        error(stmt.loc, "break statement is only allowed inside a loop");
    }
}

void SemanticAnalyzer::checkContinue(ContinueStmt& stmt) {
    if (loopDepth_ == 0) {
        error(stmt.loc, "continue statement is only allowed inside a loop");
    }
}

bool SemanticAnalyzer::stmtAlwaysReturns(const Stmt& stmt) const {
    if (dynamic_cast<const ReturnStmt*>(&stmt)) {
        return true;
    }

    if (auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
        return blockAlwaysReturns(*block);
    }

    if (auto* ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        return ifStmt->elseBlock != nullptr && blockAlwaysReturns(*ifStmt->thenBlock) &&
               blockAlwaysReturns(*ifStmt->elseBlock);
    }

    return false;
}

bool SemanticAnalyzer::blockAlwaysReturns(const BlockStmt& block) const {
    for (const auto& statement : block.statements) {
        if (dynamic_cast<const BreakStmt*>(statement.get()) || dynamic_cast<const ContinueStmt*>(statement.get())) {
            return false;
        }
        if (stmtAlwaysReturns(*statement)) {
            return true;
        }
    }
    return false;
}

void SemanticAnalyzer::enterScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::leaveScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

bool SemanticAnalyzer::declareSymbol(const SourceLocation& loc, const std::string& name, TypeKind type) {
    if (scopes_.empty()) {
        enterScope();
    }

    auto& currentScope = scopes_.back();
    if (currentScope.find(name) != currentScope.end()) {
        error(loc, "variable '" + name + "' is already declared in this scope");
        return false;
    }

    currentScope.emplace(name, type);
    return true;
}

TypeKind SemanticAnalyzer::lookupSymbol(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) {
            return found->second;
        }
    }
    return TypeKind::Error;
}

bool SemanticAnalyzer::ensureImplementedType(const SourceLocation& loc, TypeKind type) {
    if (type == TypeKind::Float) {
        error(loc, "float is not implemented yet");
        return false;
    }
    return type != TypeKind::Error;
}

void SemanticAnalyzer::requireType(const SourceLocation& loc, TypeKind actual, TypeKind expected,
                                   const std::string& what) {
    if (actual != TypeKind::Error && actual != expected) {
        error(loc, what + " must have type " + toString(expected) + ", got " + toString(actual));
    }
}

void SemanticAnalyzer::error(const SourceLocation& loc, const std::string& message) {
    errors_.push_back(formatLocation(loc) + ": semantic error: " + message);
}

std::string SemanticAnalyzer::formatLocation(const SourceLocation& loc) const {
    std::ostringstream stream;
    if (!loc.file.empty()) {
        stream << loc.file << ':';
    }
    stream << loc.line << ':' << loc.column;
    return stream.str();
}

} // namespace mini
