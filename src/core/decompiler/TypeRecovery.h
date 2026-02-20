// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#pragma once

#include "../ir/IRFunction.h"
#include "../ir/IRType.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace VBDecompiler {

/**
 * @brief Type Recovery Engine
 * 
 * Infers VB types from IR expressions and operations.
 * Uses constraint-based type inference to determine the most specific
 * type for each variable and expression.
 * 
 * Type inference rules:
 * - Literal constants determine immediate types (10 -> Integer, "abc" -> String)
 * - Operations constrain types (Integer + Integer -> Integer)
 * - Function calls may return specific types
 * - Default to Variant if type cannot be determined
 */
class TypeRecovery {
public:
    TypeRecovery() = default;
    
    /**
     * @brief Perform type inference on an IR function
     * @param function IR function to analyze
     */
    void analyzeFunction(IRFunction& function);
    
    /**
     * @brief Get inferred type for a variable
     * @param variableId Variable ID
     * @return Inferred type, or Variant if unknown
     */
    IRType getVariableType(uint32_t variableId) const;
    
    /**
     * @brief Get inferred type for an expression
     * @param expr Expression to analyze
     * @return Inferred type
     */
    IRType inferExpressionType(const IRExpression* expr) const;
    
    /**
     * @brief Clear all type information
     */
    void clear();

private:
    // Type constraint analysis
    void analyzeStatement(const IRStatement* stmt);
    void analyzeExpression(const IRExpression* expr);
    
    // Constraint propagation
    void propagateConstraints();
    
    // Type unification (merge types based on constraints)
    IRType unifyTypes(const IRType& type1, const IRType& type2) const;
    
    // Helper: Infer binary operation result type
    IRType inferBinaryOpType(IRExpressionKind op, const IRType& leftType, const IRType& rightType) const;
    
    // Helper: Infer unary operation result type
    IRType inferUnaryOpType(IRExpressionKind op, const IRType& operandType) const;
    
    // Type constraints: variableId -> inferred type
    std::unordered_map<uint32_t, IRType> variableTypes_;
    
    // Current function being analyzed
    IRFunction* currentFunction_ = nullptr;
};

} // namespace VBDecompiler
