#include "codegen.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetOptions.h>
#if __has_include(<llvm/TargetParser/Triple.h>)
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#endif

#include <optional>
#include <sstream>
#include <system_error>

namespace mini {

CodeGenerator::CodeGenerator(std::string targetTriple)
    : builder_(context_), targetTriple_(std::move(targetTriple)) {}

bool CodeGenerator::generate(const Program& program, const std::string& objectPath, const std::string& requestedIrPath) {
    errors_.clear();
    scopes_.clear();
    loopStack_.clear();
    currentFunction_ = nullptr;

    static const bool targetsInitialized = [] {
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();
        return true;
    }();
    (void)targetsInitialized;

    auto targetMachine = createTargetMachine();
    if (!targetMachine) {
        return false;
    }

    module_ = std::make_unique<llvm::Module>("mini_compiler_module", context_);
#if LLVM_VERSION_MAJOR >= 21
    module_->setTargetTriple(llvm::Triple(targetTriple_));
#else
    module_->setTargetTriple(targetTriple_);
#endif
    module_->setDataLayout(targetMachine->createDataLayout());

    for (const auto& function : program.functions) {
        emitFunction(*function);
    }

    if (hasErrors() || !verifyGeneratedModule()) {
        return false;
    }

    const std::string irPath = requestedIrPath.empty() ? defaultIrPathForObject(objectPath) : requestedIrPath;
    if (!writeIrFile(irPath)) {
        return false;
    }

    return emitObjectFile(*targetMachine, objectPath);
}

const std::vector<std::string>& CodeGenerator::errors() const {
    return errors_;
}

bool CodeGenerator::hasErrors() const {
    return !errors_.empty();
}

void CodeGenerator::emitFunction(const FunctionDecl& functionDecl) {
    std::vector<llvm::Type*> paramTypes;
    paramTypes.reserve(functionDecl.params.size());
    for (const Param& param : functionDecl.params) {
        paramTypes.push_back(llvmType(param.type));
    }

    auto* functionType = llvm::FunctionType::get(llvmType(functionDecl.returnType), paramTypes, false);
    const auto linkage = functionDecl.name == "compiled_fn" ? llvm::Function::ExternalLinkage
                                                            : llvm::Function::InternalLinkage;
    auto* function =
        llvm::Function::Create(functionType, linkage, functionDecl.name, module_.get());
    currentFunction_ = function;

    auto* entry = llvm::BasicBlock::Create(context_, "entry", function);
    builder_.SetInsertPoint(entry);

    enterScope();
    std::size_t index = 0;
    for (llvm::Argument& arg : function->args()) {
        const Param& param = functionDecl.params[index++];
        arg.setName(param.name);
        llvm::AllocaInst* alloca = createEntryAlloca(function, param.name, param.type);
        builder_.CreateStore(&arg, alloca);
        bindSymbol(param.name, alloca);
    }

    emitBlock(*functionDecl.body, false);

    if (builder_.GetInsertBlock() && !builder_.GetInsertBlock()->getTerminator()) {
        if (functionDecl.returnType == TypeKind::Int) {
            builder_.CreateRet(zeroValue(TypeKind::Int));
        } else {
            builder_.CreateUnreachable();
        }
    }

    std::string verifierError;
    llvm::raw_string_ostream stream(verifierError);
    if (llvm::verifyFunction(*function, &stream)) {
        error(functionDecl.loc, "generated invalid LLVM function: " + stream.str());
    }

    leaveScope();
    currentFunction_ = nullptr;
    builder_.ClearInsertionPoint();
}

void CodeGenerator::emitBlock(const BlockStmt& block, bool createScope) {
    if (createScope) {
        enterScope();
    }

    for (const auto& statement : block.statements) {
        if (!builder_.GetInsertBlock() || builder_.GetInsertBlock()->getTerminator()) {
            break;
        }
        emitStmt(*statement);
    }

    if (createScope) {
        leaveScope();
    }
}

void CodeGenerator::emitStmt(const Stmt& stmt) {
    if (!builder_.GetInsertBlock() || builder_.GetInsertBlock()->getTerminator()) {
        return;
    }

    if (auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
        emitBlock(*block, true);
    } else if (auto* varDecl = dynamic_cast<const VarDeclStmt*>(&stmt)) {
        emitVarDecl(*varDecl);
    } else if (auto* assign = dynamic_cast<const AssignStmt*>(&stmt)) {
        emitAssign(*assign);
    } else if (auto* ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        emitIf(*ifStmt);
    } else if (auto* forStmt = dynamic_cast<const ForStmt*>(&stmt)) {
        emitFor(*forStmt);
    } else if (auto* doWhileStmt = dynamic_cast<const DoWhileStmt*>(&stmt)) {
        emitDoWhile(*doWhileStmt);
    } else if (auto* returnStmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
        emitReturn(*returnStmt);
    } else if (dynamic_cast<const BreakStmt*>(&stmt)) {
        builder_.CreateBr(loopStack_.back().breakBlock);
    } else if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        builder_.CreateBr(loopStack_.back().continueBlock);
    } else if (auto* exprStmt = dynamic_cast<const ExprStmt*>(&stmt)) {
        emitExpr(*exprStmt->expr);
    }
}

void CodeGenerator::emitVarDecl(const VarDeclStmt& stmt) {
    llvm::AllocaInst* alloca = createEntryAlloca(currentFunction_, stmt.name, stmt.declaredType);
    bindSymbol(stmt.name, alloca);

    llvm::Value* initialValue = stmt.initializer ? emitExpr(*stmt.initializer) : zeroValue(stmt.declaredType);
    builder_.CreateStore(initialValue, alloca);
}

void CodeGenerator::emitAssign(const AssignStmt& stmt) {
    llvm::AllocaInst* alloca = lookupSymbol(stmt.name);
    if (!alloca) {
        error(stmt.loc, "internal codegen error: unknown variable '" + stmt.name + "'");
        return;
    }
    llvm::Value* value = emitExpr(*stmt.value);
    builder_.CreateStore(value, alloca);
}

void CodeGenerator::emitIf(const IfStmt& stmt) {
    llvm::Value* condition = emitExpr(*stmt.condition);
    auto* thenBlock = llvm::BasicBlock::Create(context_, "if.then", currentFunction_);
    auto* mergeBlock = llvm::BasicBlock::Create(context_, "if.end", currentFunction_);
    llvm::BasicBlock* elseBlock =
        stmt.elseBlock ? llvm::BasicBlock::Create(context_, "if.else", currentFunction_) : mergeBlock;

    builder_.CreateCondBr(condition, thenBlock, elseBlock);

    builder_.SetInsertPoint(thenBlock);
    emitBlock(*stmt.thenBlock, true);
    const bool thenTerminated = !builder_.GetInsertBlock() || builder_.GetInsertBlock()->getTerminator();
    if (!thenTerminated) {
        builder_.CreateBr(mergeBlock);
    }

    bool elseTerminated = false;
    if (stmt.elseBlock) {
        builder_.SetInsertPoint(elseBlock);
        emitBlock(*stmt.elseBlock, true);
        elseTerminated = !builder_.GetInsertBlock() || builder_.GetInsertBlock()->getTerminator();
        if (!elseTerminated) {
            builder_.CreateBr(mergeBlock);
        }
    }

    if (!thenTerminated || !elseTerminated) {
        builder_.SetInsertPoint(mergeBlock);
    } else {
        mergeBlock->eraseFromParent();
        builder_.ClearInsertionPoint();
    }
}

void CodeGenerator::emitFor(const ForStmt& stmt) {
    auto* initBlock = llvm::BasicBlock::Create(context_, "for.init", currentFunction_);
    auto* conditionBlock = llvm::BasicBlock::Create(context_, "for.cond", currentFunction_);
    auto* bodyBlock = llvm::BasicBlock::Create(context_, "for.body", currentFunction_);
    auto* stepBlock = llvm::BasicBlock::Create(context_, "for.step", currentFunction_);
    auto* endBlock = llvm::BasicBlock::Create(context_, "for.end", currentFunction_);

    enterScope();
    builder_.CreateBr(initBlock);

    builder_.SetInsertPoint(initBlock);
    if (stmt.init) {
        emitStmt(*stmt.init);
    }
    if (builder_.GetInsertBlock() && !builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(conditionBlock);
    }

    builder_.SetInsertPoint(conditionBlock);
    if (stmt.condition) {
        llvm::Value* condition = emitExpr(*stmt.condition);
        builder_.CreateCondBr(condition, bodyBlock, endBlock);
    } else {
        builder_.CreateBr(bodyBlock);
    }

    builder_.SetInsertPoint(bodyBlock);
    loopStack_.push_back(LoopTargets{endBlock, stepBlock});
    emitBlock(*stmt.body, true);
    loopStack_.pop_back();
    if (builder_.GetInsertBlock() && !builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(stepBlock);
    }

    builder_.SetInsertPoint(stepBlock);
    if (stmt.step) {
        emitStmt(*stmt.step);
    }
    if (builder_.GetInsertBlock() && !builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(conditionBlock);
    }

    builder_.SetInsertPoint(endBlock);
    leaveScope();
}

void CodeGenerator::emitDoWhile(const DoWhileStmt& stmt) {
    auto* bodyBlock = llvm::BasicBlock::Create(context_, "do.body", currentFunction_);
    auto* conditionBlock = llvm::BasicBlock::Create(context_, "do.cond", currentFunction_);
    auto* endBlock = llvm::BasicBlock::Create(context_, "do.end", currentFunction_);

    // do/while executes the body before the first condition check. A continue jumps
    // to do.cond, while a break jumps to do.end.
    builder_.CreateBr(bodyBlock);

    builder_.SetInsertPoint(bodyBlock);
    loopStack_.push_back(LoopTargets{endBlock, conditionBlock});
    emitBlock(*stmt.body, true);
    loopStack_.pop_back();

    if (builder_.GetInsertBlock() && !builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(conditionBlock);
    }

    builder_.SetInsertPoint(conditionBlock);
    llvm::Value* condition = emitExpr(*stmt.condition);
    builder_.CreateCondBr(condition, bodyBlock, endBlock);

    builder_.SetInsertPoint(endBlock);
}

void CodeGenerator::emitReturn(const ReturnStmt& stmt) {
    llvm::Value* value = emitExpr(*stmt.value);
    builder_.CreateRet(value);
}

llvm::Value* CodeGenerator::emitExpr(const Expr& expr) {
    if (auto* literal = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        return llvm::ConstantInt::get(llvmType(TypeKind::Int), literal->value, true);
    }

    if (auto* var = dynamic_cast<const VarExpr*>(&expr)) {
        llvm::AllocaInst* alloca = lookupSymbol(var->name);
        if (!alloca) {
            error(var->loc, "internal codegen error: unknown variable '" + var->name + "'");
            return zeroValue(TypeKind::Int);
        }
        return builder_.CreateLoad(llvmType(var->inferredType), alloca, var->name + ".load");
    }

    if (auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
        return emitUnary(*unary);
    }

    if (auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
        return emitBinary(*binary);
    }

    error(expr.loc, "internal codegen error: unsupported expression");
    return zeroValue(TypeKind::Int);
}

llvm::Value* CodeGenerator::emitUnary(const UnaryExpr& expr) {
    llvm::Value* operand = emitExpr(*expr.operand);
    switch (expr.op) {
    case UnaryOp::Negate:
        return builder_.CreateNeg(operand, "negtmp");
    case UnaryOp::LogicalNot:
        return builder_.CreateNot(operand, "nottmp");
    }
    error(expr.loc, "internal codegen error: unsupported unary operator");
    return zeroValue(TypeKind::Int);
}

llvm::Value* CodeGenerator::emitBinary(const BinaryExpr& expr) {
    if (expr.op == BinaryOp::LogicalAnd) {
        return emitLogicalAnd(expr);
    }
    if (expr.op == BinaryOp::LogicalOr) {
        return emitLogicalOr(expr);
    }

    llvm::Value* lhs = emitExpr(*expr.lhs);
    llvm::Value* rhs = emitExpr(*expr.rhs);

    switch (expr.op) {
    case BinaryOp::Add:
        return builder_.CreateAdd(lhs, rhs, "addtmp");
    case BinaryOp::Subtract:
        return builder_.CreateSub(lhs, rhs, "subtmp");
    case BinaryOp::Multiply:
        return builder_.CreateMul(lhs, rhs, "multmp");
    case BinaryOp::Divide:
        return builder_.CreateSDiv(lhs, rhs, "divtmp");
    case BinaryOp::Modulo:
        return builder_.CreateSRem(lhs, rhs, "modtmp");
    case BinaryOp::Equal:
        return builder_.CreateICmpEQ(lhs, rhs, "eqtmp");
    case BinaryOp::NotEqual:
        return builder_.CreateICmpNE(lhs, rhs, "netmp");
    case BinaryOp::Less:
        return builder_.CreateICmpSLT(lhs, rhs, "lttmp");
    case BinaryOp::LessEqual:
        return builder_.CreateICmpSLE(lhs, rhs, "letmp");
    case BinaryOp::Greater:
        return builder_.CreateICmpSGT(lhs, rhs, "gttmp");
    case BinaryOp::GreaterEqual:
        return builder_.CreateICmpSGE(lhs, rhs, "getmp");
    case BinaryOp::LogicalAnd:
    case BinaryOp::LogicalOr:
        break;
    }

    error(expr.loc, "internal codegen error: unsupported binary operator");
    return zeroValue(TypeKind::Int);
}

llvm::Value* CodeGenerator::emitLogicalAnd(const BinaryExpr& expr) {
    llvm::Value* lhs = emitExpr(*expr.lhs);
    llvm::BasicBlock* lhsBlock = builder_.GetInsertBlock();
    auto* rhsBlock = llvm::BasicBlock::Create(context_, "and.rhs", currentFunction_);
    auto* mergeBlock = llvm::BasicBlock::Create(context_, "and.end", currentFunction_);

    // Short-circuit: if lhs is false, rhs is not evaluated and the PHI receives false.
    builder_.CreateCondBr(lhs, rhsBlock, mergeBlock);

    builder_.SetInsertPoint(rhsBlock);
    llvm::Value* rhs = emitExpr(*expr.rhs);
    llvm::BasicBlock* rhsEndBlock = builder_.GetInsertBlock();
    builder_.CreateBr(mergeBlock);

    builder_.SetInsertPoint(mergeBlock);
    llvm::PHINode* phi = builder_.CreatePHI(llvmType(TypeKind::Bool), 2, "andtmp");
    phi->addIncoming(llvm::ConstantInt::getFalse(context_), lhsBlock);
    phi->addIncoming(rhs, rhsEndBlock);
    return phi;
}

llvm::Value* CodeGenerator::emitLogicalOr(const BinaryExpr& expr) {
    llvm::Value* lhs = emitExpr(*expr.lhs);
    llvm::BasicBlock* lhsBlock = builder_.GetInsertBlock();
    auto* rhsBlock = llvm::BasicBlock::Create(context_, "or.rhs", currentFunction_);
    auto* mergeBlock = llvm::BasicBlock::Create(context_, "or.end", currentFunction_);

    // Short-circuit: if lhs is true, rhs is not evaluated and the PHI receives true.
    builder_.CreateCondBr(lhs, mergeBlock, rhsBlock);

    builder_.SetInsertPoint(rhsBlock);
    llvm::Value* rhs = emitExpr(*expr.rhs);
    llvm::BasicBlock* rhsEndBlock = builder_.GetInsertBlock();
    builder_.CreateBr(mergeBlock);

    builder_.SetInsertPoint(mergeBlock);
    llvm::PHINode* phi = builder_.CreatePHI(llvmType(TypeKind::Bool), 2, "ortmp");
    phi->addIncoming(llvm::ConstantInt::getTrue(context_), lhsBlock);
    phi->addIncoming(rhs, rhsEndBlock);
    return phi;
}

llvm::Type* CodeGenerator::llvmType(TypeKind type) {
    switch (type) {
    case TypeKind::Int:
        return llvm::Type::getInt64Ty(context_);
    case TypeKind::Bool:
        return llvm::Type::getInt1Ty(context_);
    case TypeKind::Float:
    case TypeKind::Void:
    case TypeKind::Error:
        break;
    }
    return llvm::Type::getVoidTy(context_);
}

llvm::Constant* CodeGenerator::zeroValue(TypeKind type) {
    if (type == TypeKind::Bool) {
        return llvm::ConstantInt::getFalse(context_);
    }
    return llvm::ConstantInt::get(llvmType(TypeKind::Int), 0, true);
}

llvm::AllocaInst* CodeGenerator::createEntryAlloca(llvm::Function* function, const std::string& name, TypeKind type) {
    llvm::IRBuilder<> temporaryBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
    return temporaryBuilder.CreateAlloca(llvmType(type), nullptr, name);
}

void CodeGenerator::enterScope() {
    scopes_.emplace_back();
}

void CodeGenerator::leaveScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void CodeGenerator::bindSymbol(const std::string& name, llvm::AllocaInst* alloca) {
    if (scopes_.empty()) {
        enterScope();
    }
    scopes_.back()[name] = alloca;
}

llvm::AllocaInst* CodeGenerator::lookupSymbol(const std::string& name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) {
            return found->second;
        }
    }
    return nullptr;
}

std::unique_ptr<llvm::TargetMachine> CodeGenerator::createTargetMachine() {
    std::string targetError;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple_, targetError);
    if (!target) {
        error("cannot create target '" + targetTriple_ + "': " + targetError);
        return nullptr;
    }

    llvm::TargetOptions options;
#if LLVM_VERSION_MAJOR >= 21
    auto* targetMachine = target->createTargetMachine(llvm::Triple(targetTriple_), "generic", "", options,
                                                      std::nullopt, std::nullopt,
                                                      llvm::CodeGenOptLevel::Default, false);
#else
    auto* targetMachine = target->createTargetMachine(targetTriple_, "generic", "", options);
#endif
    if (!targetMachine) {
        error("target '" + targetTriple_ + "' is available, but LLVM failed to create TargetMachine");
        return nullptr;
    }

    return std::unique_ptr<llvm::TargetMachine>(targetMachine);
}

bool CodeGenerator::verifyGeneratedModule() {
    std::string verifierError;
    llvm::raw_string_ostream stream(verifierError);
    if (llvm::verifyModule(*module_, &stream)) {
        error("generated invalid LLVM module: " + stream.str());
        return false;
    }
    return true;
}

bool CodeGenerator::writeIrFile(const std::string& irPath) {
    std::error_code ec;
    llvm::raw_fd_ostream output(irPath, ec, llvm::sys::fs::OF_Text);
    if (ec) {
        error("cannot write LLVM IR file '" + irPath + "': " + ec.message());
        return false;
    }

    module_->print(output, nullptr);
    output.flush();
    return true;
}

bool CodeGenerator::emitObjectFile(llvm::TargetMachine& targetMachine, const std::string& objectPath) {
    std::error_code ec;
    llvm::raw_fd_ostream output(objectPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        error("cannot write object file '" + objectPath + "': " + ec.message());
        return false;
    }

    llvm::legacy::PassManager passManager;
    // TargetMachine owns the final lowering from verified LLVM IR to the requested x86 object format.
#if LLVM_VERSION_MAJOR >= 18
    const auto fileType = llvm::CodeGenFileType::ObjectFile;
#else
    const auto fileType = llvm::CGFT_ObjectFile;
#endif
    if (targetMachine.addPassesToEmitFile(passManager, output, nullptr, fileType)) {
        error("target '" + targetTriple_ + "' cannot emit object files");
        return false;
    }

    passManager.run(*module_);
    output.flush();
    return true;
}

std::string CodeGenerator::defaultIrPathForObject(const std::string& objectPath) const {
    const std::size_t lastSeparator = objectPath.find_last_of("/\\");
    const std::size_t lastDot = objectPath.find_last_of('.');
    if (lastDot != std::string::npos && (lastSeparator == std::string::npos || lastDot > lastSeparator)) {
        return objectPath.substr(0, lastDot) + ".ll";
    }
    return objectPath + ".ll";
}

void CodeGenerator::error(const SourceLocation& loc, const std::string& message) {
    errors_.push_back(formatLocation(loc) + ": codegen error: " + message);
}

void CodeGenerator::error(const std::string& message) {
    errors_.push_back("codegen error: " + message);
}

std::string CodeGenerator::formatLocation(const SourceLocation& loc) const {
    std::ostringstream stream;
    if (!loc.file.empty()) {
        stream << loc.file << ':';
    }
    stream << loc.line << ':' << loc.column;
    return stream.str();
}

} // namespace mini
