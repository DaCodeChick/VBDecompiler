// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "TypeRecovery.h"

namespace VBDecompiler {

void TypeRecovery::analyzeFunction(IRFunction& function) {
    currentFunction_ = &function;
    variableTypes_.clear();
    
    // Initialize parameter types
    for (const auto& param : function.getParameters()) {
        variableTypes_.insert({param.getId(), param.getType()});
    }
    
    // Initialize local variable types
    for (const auto& local : function.getLocalVariables()) {
        variableTypes_.insert({local.getId(), local.getType()});
    }
    
    // Analyze all basic blocks
    for (const auto& [blockId, block] : function.getBasicBlocks()) {
        for (const auto& stmt : block->getStatements()) {
            analyzeStatement(stmt.get());
        }
    }
    
    // Propagate type constraints
    propagateConstraints();
    
    // Note: We don't update the function's parameter/local types directly
    // since they are stored by value. The type information is maintained
    // in variableTypes_ map and queried via getVariableType().
    // TODO: If we need to update the IRFunction's variables, we need
    // to provide setter methods in IRFunction class.
}

IRType TypeRecovery::getVariableType(uint32_t variableId) const {
    auto it = variableTypes_.find(variableId);
    if (it != variableTypes_.end()) {
        return it->second;
    }
    return IRTypes::Variant;  // Default to Variant
}

IRType TypeRecovery::inferExpressionType(const IRExpression* expr) const {
    if (!expr) {
        return IRTypes::Variant;
    }
    
    switch (expr->getKind()) {
        case IRExpressionKind::CONSTANT: {
            // Constants have explicit types
            const auto* constant = expr->getConstant();
            if (constant) {
                return constant->getType();
            }
            break;
        }
        
        case IRExpressionKind::VARIABLE: {
            // Look up variable type
            const auto* variable = expr->getVariable();
            if (variable) {
                return getVariableType(variable->getId());
            }
            break;
        }
        
        case IRExpressionKind::NEGATE:
        case IRExpressionKind::NOT: {
            // Unary operations
            const auto* operand = expr->getOperand();
            if (operand) {
                IRType operandType = inferExpressionType(operand);
                return inferUnaryOpType(expr->getKind(), operandType);
            }
            break;
        }
        
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
        case IRExpressionKind::CONCATENATE: {
            // Binary operations
            const auto* left = expr->getLeft();
            const auto* right = expr->getRight();
            if (left && right) {
                IRType leftType = inferExpressionType(left);
                IRType rightType = inferExpressionType(right);
                return inferBinaryOpType(expr->getKind(), leftType, rightType);
            }
            break;
        }
        
        case IRExpressionKind::CALL: {
            // Function calls - for now, return Variant
            // TODO: Implement function signature lookup
            return IRTypes::Variant;
        }
        
        case IRExpressionKind::MEMBER_ACCESS:
        case IRExpressionKind::ARRAY_INDEX: {
            // Complex expressions - default to Variant
            return IRTypes::Variant;
        }
        
        case IRExpressionKind::CAST: {
            // Cast has explicit target type
            return expr->getType();
        }
        
        default:
            break;
    }
    
    return IRTypes::Variant;
}

void TypeRecovery::clear() {
    variableTypes_.clear();
    currentFunction_ = nullptr;
}

void TypeRecovery::analyzeStatement(const IRStatement* stmt) {
    if (!stmt) {
        return;
    }
    
    switch (stmt->getKind()) {
        case IRStatementKind::ASSIGN: {
            // Assignment: target = value
            const auto* target = stmt->getTarget();
            const auto* value = stmt->getValue();
            
            if (target && value) {
                IRType valueType = inferExpressionType(value);
                IRType targetType = getVariableType(target->getId());
                
                // Unify types: if target is Variant, use value type
                if (targetType.getKind() == VBTypeKind::VARIANT) {
                    variableTypes_.insert_or_assign(target->getId(), valueType);
                } else {
                    // Keep target type (explicit type annotation wins)
                    variableTypes_.insert_or_assign(target->getId(), targetType);
                }
            }
            break;
        }
        
        case IRStatementKind::BRANCH: {
            // Analyze condition expression
            const auto* condition = stmt->getCondition();
            if (condition) {
                analyzeExpression(condition);
            }
            break;
        }
        
        case IRStatementKind::RETURN: {
            // Analyze return value
            const auto* value = stmt->getValue();
            if (value) {
                analyzeExpression(value);
            }
            break;
        }
        
        case IRStatementKind::CALL: {
            // Analyze call arguments
            // Note: IRStatement::makeCall doesn't store arguments currently
            // This would need to be extended
            break;
        }
        
        default:
            break;
    }
}

void TypeRecovery::analyzeExpression(const IRExpression* expr) {
    if (!expr) {
        return;
    }
    
    // Recursively analyze sub-expressions
    switch (expr->getKind()) {
        case IRExpressionKind::NEGATE:
        case IRExpressionKind::NOT:
            if (expr->getOperand()) {
                analyzeExpression(expr->getOperand());
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
            if (expr->getLeft()) {
                analyzeExpression(expr->getLeft());
            }
            if (expr->getRight()) {
                analyzeExpression(expr->getRight());
            }
            break;
            
        case IRExpressionKind::CALL:
            // Analyze call arguments
            for (const auto& arg : expr->getArguments()) {
                analyzeExpression(arg.get());
            }
            break;
            
        default:
            break;
    }
}

void TypeRecovery::propagateConstraints() {
    // Simple constraint propagation
    // In a full implementation, this would use a worklist algorithm
    // For now, one pass is sufficient
}

IRType TypeRecovery::unifyTypes(const IRType& type1, const IRType& type2) const {
    // If either is Variant, use the other
    if (type1.getKind() == VBTypeKind::VARIANT) {
        return type2;
    }
    if (type2.getKind() == VBTypeKind::VARIANT) {
        return type1;
    }
    
    // If types match, return either
    if (type1 == type2) {
        return type1;
    }
    
    // Numeric type promotion rules
    // Integer + Long -> Long
    // Integer + Single -> Single
    // etc.
    
    auto kind1 = type1.getKind();
    auto kind2 = type2.getKind();
    
    // Promote to larger numeric type
    if (type1.isNumeric() && type2.isNumeric()) {
        // Double > Single > Currency > Long > Integer > Byte
        if (kind1 == VBTypeKind::DOUBLE || kind2 == VBTypeKind::DOUBLE) {
            return IRTypes::Double;
        }
        if (kind1 == VBTypeKind::SINGLE || kind2 == VBTypeKind::SINGLE) {
            return IRTypes::Single;
        }
        if (kind1 == VBTypeKind::CURRENCY || kind2 == VBTypeKind::CURRENCY) {
            return IRTypes::Currency;
        }
        if (kind1 == VBTypeKind::LONG || kind2 == VBTypeKind::LONG) {
            return IRTypes::Long;
        }
        if (kind1 == VBTypeKind::INTEGER || kind2 == VBTypeKind::INTEGER) {
            return IRTypes::Integer;
        }
        return IRTypes::Byte;
    }
    
    // String concatenation
    if (kind1 == VBTypeKind::STRING || kind2 == VBTypeKind::STRING) {
        return IRTypes::String;
    }
    
    // Default to Variant for incompatible types
    return IRTypes::Variant;
}

IRType TypeRecovery::inferBinaryOpType(IRExpressionKind op, const IRType& leftType, const IRType& rightType) const {
    switch (op) {
        case IRExpressionKind::ADD:
        case IRExpressionKind::SUBTRACT:
        case IRExpressionKind::MULTIPLY:
        case IRExpressionKind::DIVIDE:
            // Arithmetic: promote to common numeric type
            return unifyTypes(leftType, rightType);
            
        case IRExpressionKind::INT_DIVIDE:
        case IRExpressionKind::MODULO:
            // Integer division/modulo: result is Long
            return IRTypes::Long;
            
        case IRExpressionKind::CONCATENATE:
            // String concatenation
            return IRTypes::String;
            
        case IRExpressionKind::EQUAL:
        case IRExpressionKind::NOT_EQUAL:
        case IRExpressionKind::LESS_THAN:
        case IRExpressionKind::LESS_EQUAL:
        case IRExpressionKind::GREATER_THAN:
        case IRExpressionKind::GREATER_EQUAL:
            // Comparisons return Boolean
            return IRTypes::Boolean;
            
        case IRExpressionKind::AND:
        case IRExpressionKind::OR:
        case IRExpressionKind::XOR:
            // Logical operations return Boolean
            return IRTypes::Boolean;
            
        default:
            return IRTypes::Variant;
    }
}

IRType TypeRecovery::inferUnaryOpType(IRExpressionKind op, const IRType& operandType) const {
    switch (op) {
        case IRExpressionKind::NEGATE:
            // Negation preserves numeric type
            if (operandType.isNumeric()) {
                return operandType;
            }
            return IRTypes::Variant;
            
        case IRExpressionKind::NOT:
            // Logical NOT returns Boolean
            return IRTypes::Boolean;
            
        default:
            return IRTypes::Variant;
    }
}

} // namespace VBDecompiler
