// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "IRStatement.h"
#include <sstream>

namespace VBDecompiler {

// Static factory methods

std::unique_ptr<IRStatement> IRStatement::makeAssign(
    IRVariable target,
    std::unique_ptr<IRExpression> value
) {
    auto stmt = std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::ASSIGN));
    stmt->data_.target = std::make_unique<IRVariable>(std::move(target));
    stmt->data_.value = std::move(value);
    return stmt;
}

std::unique_ptr<IRStatement> IRStatement::makeStore(
    std::unique_ptr<IRExpression> address,
    std::unique_ptr<IRExpression> value
) {
    auto stmt = std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::STORE));
    stmt->data_.address = std::move(address);
    stmt->data_.storeValue = std::move(value);
    return stmt;
}

std::unique_ptr<IRStatement> IRStatement::makeCall(
    std::string functionName,
    std::vector<std::unique_ptr<IRExpression>> arguments
) {
    auto stmt = std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::CALL));
    stmt->data_.functionName = std::move(functionName);
    stmt->data_.arguments = std::move(arguments);
    return stmt;
}

std::unique_ptr<IRStatement> IRStatement::makeReturn(
    std::unique_ptr<IRExpression> value
) {
    auto stmt = std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::RETURN));
    stmt->data_.returnValue = std::move(value);
    return stmt;
}

std::unique_ptr<IRStatement> IRStatement::makeBranch(
    std::unique_ptr<IRExpression> condition,
    uint32_t targetBlockId
) {
    auto stmt = std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::BRANCH));
    stmt->data_.condition = std::move(condition);
    stmt->data_.targetBlockId = targetBlockId;
    return stmt;
}

std::unique_ptr<IRStatement> IRStatement::makeGoto(uint32_t targetBlockId) {
    auto stmt = std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::GOTO));
    stmt->data_.gotoTarget = targetBlockId;
    return stmt;
}

std::unique_ptr<IRStatement> IRStatement::makeLabel(uint32_t labelId) {
    auto stmt = std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::LABEL));
    stmt->data_.labelId = labelId;
    return stmt;
}

std::unique_ptr<IRStatement> IRStatement::makeNop() {
    return std::unique_ptr<IRStatement>(new IRStatement(IRStatementKind::NOP));
}

// Getters

const IRVariable* IRStatement::getTarget() const {
    return (kind_ == IRStatementKind::ASSIGN) ? data_.target.get() : nullptr;
}

const IRExpression* IRStatement::getValue() const {
    return (kind_ == IRStatementKind::ASSIGN) ? data_.value.get() : nullptr;
}

const IRExpression* IRStatement::getAddress() const {
    return (kind_ == IRStatementKind::STORE) ? data_.address.get() : nullptr;
}

const IRExpression* IRStatement::getStoreValue() const {
    return (kind_ == IRStatementKind::STORE) ? data_.storeValue.get() : nullptr;
}

const std::string& IRStatement::getFunctionName() const {
    return data_.functionName;
}

const std::vector<std::unique_ptr<IRExpression>>& IRStatement::getArguments() const {
    return data_.arguments;
}

const IRExpression* IRStatement::getReturnValue() const {
    return (kind_ == IRStatementKind::RETURN) ? data_.returnValue.get() : nullptr;
}

const IRExpression* IRStatement::getCondition() const {
    return (kind_ == IRStatementKind::BRANCH) ? data_.condition.get() : nullptr;
}

uint32_t IRStatement::getTargetBlockId() const {
    return (kind_ == IRStatementKind::BRANCH) ? data_.targetBlockId : 0;
}

uint32_t IRStatement::getGotoTarget() const {
    return (kind_ == IRStatementKind::GOTO) ? data_.gotoTarget : 0;
}

uint32_t IRStatement::getLabelId() const {
    return (kind_ == IRStatementKind::LABEL) ? data_.labelId : 0;
}

// toString() implementation
std::string IRStatement::toString() const {
    std::ostringstream oss;
    
    switch (kind_) {
        case IRStatementKind::ASSIGN:
            if (data_.target && data_.value) {
                oss << data_.target->toString() << " = " << data_.value->toString();
            }
            break;
            
        case IRStatementKind::STORE:
            if (data_.address && data_.storeValue) {
                oss << "[" << data_.address->toString() << "] = " 
                    << data_.storeValue->toString();
            }
            break;
            
        case IRStatementKind::CALL:
            oss << data_.functionName << "(";
            for (size_t i = 0; i < data_.arguments.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << data_.arguments[i]->toString();
            }
            oss << ")";
            break;
            
        case IRStatementKind::RETURN:
            oss << "Return";
            if (data_.returnValue) {
                oss << " " << data_.returnValue->toString();
            }
            break;
            
        case IRStatementKind::BRANCH:
            if (data_.condition) {
                oss << "If " << data_.condition->toString() 
                    << " Then Goto BB" << data_.targetBlockId;
            }
            break;
            
        case IRStatementKind::GOTO:
            oss << "Goto BB" << data_.gotoTarget;
            break;
            
        case IRStatementKind::LABEL:
            oss << "Label_" << data_.labelId << ":";
            break;
            
        case IRStatementKind::NOP:
            oss << "Nop";
            break;
    }
    
    return oss.str();
}

} // namespace VBDecompiler
