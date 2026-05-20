#pragma once

#include "ast.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

namespace mini {

class CodeGenerator {
public:
    explicit CodeGenerator(std::string targetTriple = "x86_64-pc-linux-gnu");

    bool generate(const Program& program, const std::string& objectPath, const std::string& requestedIrPath);

    const std::vector<std::string>& errors() const;
    bool hasErrors() const;

private:
    struct LoopTargets {
        llvm::BasicBlock* breakBlock = nullptr;
        llvm::BasicBlock* continueBlock = nullptr;
    };

    void emitFunction(const FunctionDecl& functionDecl);
    void emitBlock(const BlockStmt& block, bool createScope);
    void emitStmt(const Stmt& stmt);
    void emitVarDecl(const VarDeclStmt& stmt);
    void emitAssign(const AssignStmt& stmt);
    void emitIf(const IfStmt& stmt);
    void emitFor(const ForStmt& stmt);
    void emitDoWhile(const DoWhileStmt& stmt);
    void emitReturn(const ReturnStmt& stmt);

    llvm::Value* emitExpr(const Expr& expr);
    llvm::Value* emitUnary(const UnaryExpr& expr);
    llvm::Value* emitBinary(const BinaryExpr& expr);
    llvm::Value* emitLogicalAnd(const BinaryExpr& expr);
    llvm::Value* emitLogicalOr(const BinaryExpr& expr);

    llvm::Type* llvmType(TypeKind type);
    llvm::Constant* zeroValue(TypeKind type);
    llvm::AllocaInst* createEntryAlloca(llvm::Function* function, const std::string& name, TypeKind type);

    void enterScope();
    void leaveScope();
    void bindSymbol(const std::string& name, llvm::AllocaInst* alloca);
    llvm::AllocaInst* lookupSymbol(const std::string& name);

    std::unique_ptr<llvm::TargetMachine> createTargetMachine();
    bool verifyGeneratedModule();
    bool writeIrFile(const std::string& irPath);
    bool emitObjectFile(llvm::TargetMachine& targetMachine, const std::string& objectPath);
    std::string defaultIrPathForObject(const std::string& objectPath) const;

    void error(const SourceLocation& loc, const std::string& message);
    void error(const std::string& message);
    std::string formatLocation(const SourceLocation& loc) const;

    llvm::LLVMContext context_;
    llvm::IRBuilder<> builder_;
    std::unique_ptr<llvm::Module> module_;
    std::string targetTriple_;
    llvm::Function* currentFunction_ = nullptr;
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> scopes_;
    std::vector<LoopTargets> loopStack_;
    std::vector<std::string> errors_;
};

} // namespace mini
