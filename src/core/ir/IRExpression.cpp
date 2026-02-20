// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "IRExpression.h"
#include <sstream>

namespace VBDecompiler {

// IRConstant::toString()
std::string IRConstant::toString() const {
    std::ostringstream oss;
    
    std::visit([&oss](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            oss << value;
        } else if constexpr (std::is_same_v<T, double>) {
            oss << value;
        } else if constexpr (std::is_same_v<T, std::string>) {
            oss << "\"" << value << "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
            oss << (value ? "True" : "False");
        }
    }, value_);
    
    return oss.str();
}

// IRExpression static factory methods

std::unique_ptr<IRExpression> IRExpression::makeConstant(const IRConstant& constant) {
    auto expr = std::unique_ptr<IRExpression>(
        new IRExpression(IRExpressionKind::CONSTANT, constant.getType())
    );
    expr->data_.constant = std::make_unique<IRConstant>(constant);
    return expr;
}

std::unique_ptr<IRExpression> IRExpression::makeVariable(const IRVariable& variable) {
    auto expr = std::unique_ptr<IRExpression>(
        new IRExpression(IRExpressionKind::VARIABLE, variable.getType())
    );
    expr->data_.variable = std::make_unique<IRVariable>(variable);
    return expr;
}

std::unique_ptr<IRExpression> IRExpression::makeUnary(
    IRExpressionKind op,
    std::unique_ptr<IRExpression> operand,
    const IRType& resultType
) {
    auto expr = std::unique_ptr<IRExpression>(new IRExpression(op, resultType));
    expr->data_.operand = std::move(operand);
    return expr;
}

std::unique_ptr<IRExpression> IRExpression::makeBinary(
    IRExpressionKind op,
    std::unique_ptr<IRExpression> left,
    std::unique_ptr<IRExpression> right,
    const IRType& resultType
) {
    auto expr = std::unique_ptr<IRExpression>(new IRExpression(op, resultType));
    expr->data_.left = std::move(left);
    expr->data_.right = std::move(right);
    return expr;
}

std::unique_ptr<IRExpression> IRExpression::makeCall(
    std::string functionName,
    std::vector<std::unique_ptr<IRExpression>> arguments,
    const IRType& resultType
) {
    auto expr = std::unique_ptr<IRExpression>(
        new IRExpression(IRExpressionKind::CALL, resultType)
    );
    expr->data_.functionName = std::move(functionName);
    expr->data_.arguments = std::move(arguments);
    return expr;
}

std::unique_ptr<IRExpression> IRExpression::makeMemberAccess(
    std::unique_ptr<IRExpression> object,
    std::string memberName,
    const IRType& resultType
) {
    auto expr = std::unique_ptr<IRExpression>(
        new IRExpression(IRExpressionKind::MEMBER_ACCESS, resultType)
    );
    expr->data_.object = std::move(object);
    expr->data_.memberName = std::move(memberName);
    return expr;
}

std::unique_ptr<IRExpression> IRExpression::makeArrayIndex(
    std::unique_ptr<IRExpression> array,
    std::vector<std::unique_ptr<IRExpression>> indices,
    const IRType& resultType
) {
    auto expr = std::unique_ptr<IRExpression>(
        new IRExpression(IRExpressionKind::ARRAY_INDEX, resultType)
    );
    expr->data_.array = std::move(array);
    expr->data_.indices = std::move(indices);
    return expr;
}

std::unique_ptr<IRExpression> IRExpression::makeCast(
    std::unique_ptr<IRExpression> operand,
    const IRType& targetType
) {
    auto expr = std::unique_ptr<IRExpression>(
        new IRExpression(IRExpressionKind::CAST, targetType)
    );
    expr->data_.operand = std::move(operand);
    return expr;
}

// Getters

const IRConstant* IRExpression::getConstant() const {
    return (kind_ == IRExpressionKind::CONSTANT) ? data_.constant.get() : nullptr;
}

const IRVariable* IRExpression::getVariable() const {
    return (kind_ == IRExpressionKind::VARIABLE || kind_ == IRExpressionKind::TEMPORARY) 
           ? data_.variable.get() : nullptr;
}

const IRExpression* IRExpression::getOperand() const {
    return data_.operand.get();
}

const IRExpression* IRExpression::getLeft() const {
    return data_.left.get();
}

const IRExpression* IRExpression::getRight() const {
    return data_.right.get();
}

const std::string& IRExpression::getFunctionName() const {
    return data_.functionName;
}

const std::vector<std::unique_ptr<IRExpression>>& IRExpression::getArguments() const {
    return data_.arguments;
}

const IRExpression* IRExpression::getObject() const {
    return data_.object.get();
}

const std::string& IRExpression::getMemberName() const {
    return data_.memberName;
}

const IRExpression* IRExpression::getArray() const {
    return data_.array.get();
}

const std::vector<std::unique_ptr<IRExpression>>& IRExpression::getIndices() const {
    return data_.indices;
}

// Helper function to get operator string
static const char* getOperatorString(IRExpressionKind kind) {
    switch (kind) {
        case IRExpressionKind::NEGATE:         return "-";
        case IRExpressionKind::NOT:            return "Not ";
        case IRExpressionKind::ADD:            return " + ";
        case IRExpressionKind::SUBTRACT:       return " - ";
        case IRExpressionKind::MULTIPLY:       return " * ";
        case IRExpressionKind::DIVIDE:         return " / ";
        case IRExpressionKind::INT_DIVIDE:     return " \\ ";
        case IRExpressionKind::MODULO:         return " Mod ";
        case IRExpressionKind::EQUAL:          return " = ";
        case IRExpressionKind::NOT_EQUAL:      return " <> ";
        case IRExpressionKind::LESS_THAN:      return " < ";
        case IRExpressionKind::LESS_EQUAL:     return " <= ";
        case IRExpressionKind::GREATER_THAN:   return " > ";
        case IRExpressionKind::GREATER_EQUAL:  return " >= ";
        case IRExpressionKind::AND:            return " And ";
        case IRExpressionKind::OR:             return " Or ";
        case IRExpressionKind::XOR:            return " Xor ";
        case IRExpressionKind::CONCATENATE:    return " & ";
        default:                               return " ??? ";
    }
}

// toString() implementation
std::string IRExpression::toString() const {
    std::ostringstream oss;
    
    switch (kind_) {
        case IRExpressionKind::CONSTANT:
            if (data_.constant) {
                oss << data_.constant->toString();
            }
            break;
            
        case IRExpressionKind::VARIABLE:
        case IRExpressionKind::TEMPORARY:
            if (data_.variable) {
                oss << data_.variable->toString();
            }
            break;
            
        case IRExpressionKind::NEGATE:
        case IRExpressionKind::NOT:
            oss << getOperatorString(kind_);
            if (data_.operand) {
                oss << "(" << data_.operand->toString() << ")";
            }
            break;
            
        case IRExpressionKind::ADD:
        case IRExpressionKind::SUBTRACT:
        case IRExpressionKind::MULTIPLY:
        case IRExpressionKind::DIVIDE:
        case IRExpressionKind::INT_DIVIDE:
        case IRExpressionKind::MODULO:
        case IRExpressionKind::EQUAL:
        case IRExpressionKind::NOT_EQUAL:
        case IRExpressionKind::LESS_THAN:
        case IRExpressionKind::LESS_EQUAL:
        case IRExpressionKind::GREATER_THAN:
        case IRExpressionKind::GREATER_EQUAL:
        case IRExpressionKind::AND:
        case IRExpressionKind::OR:
        case IRExpressionKind::XOR:
        case IRExpressionKind::CONCATENATE:
            if (data_.left && data_.right) {
                oss << "(" << data_.left->toString() << ")";
                oss << getOperatorString(kind_);
                oss << "(" << data_.right->toString() << ")";
            }
            break;
            
        case IRExpressionKind::CALL:
            oss << data_.functionName << "(";
            for (size_t i = 0; i < data_.arguments.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << data_.arguments[i]->toString();
            }
            oss << ")";
            break;
            
        case IRExpressionKind::MEMBER_ACCESS:
            if (data_.object) {
                oss << data_.object->toString() << "." << data_.memberName;
            }
            break;
            
        case IRExpressionKind::ARRAY_INDEX:
            if (data_.array) {
                oss << data_.array->toString() << "(";
                for (size_t i = 0; i < data_.indices.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << data_.indices[i]->toString();
                }
                oss << ")";
            }
            break;
            
        case IRExpressionKind::CAST:
            if (data_.operand) {
                oss << "CType(" << data_.operand->toString() 
                    << ", " << type_.toString() << ")";
            }
            break;
            
        case IRExpressionKind::LOAD:
            if (data_.operand) {
                oss << "[" << data_.operand->toString() << "]";
            }
            break;
    }
    
    return oss.str();
}

} // namespace VBDecompiler
