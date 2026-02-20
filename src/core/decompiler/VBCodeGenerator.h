// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#pragma once

#include "../ir/IRFunction.h"
#include "ControlFlowStructurer.h"
#include "TypeRecovery.h"
#include <string>
#include <sstream>

namespace VBDecompiler {

/**
 * @brief VB6 Code Generator
 * 
 * Generates readable VB6 source code from IR and structured control flow.
 * 
 * Responsibilities:
 * - Generate Function/Sub declarations with parameters
 * - Generate Dim statements for local variables
 * - Format expressions with proper VB6 operators and precedence
 * - Format statements (assignments, calls, returns)
 * - Format structured control flow (If/Then/Else, While/Wend, etc.)
 * - Handle proper indentation
 * - Generate line continuations for long statements
 */
class VBCodeGenerator {
public:
    explicit VBCodeGenerator(const TypeRecovery& typeRecovery)
        : typeRecovery_(typeRecovery), indentLevel_(0) {}
    
    /**
     * @brief Generate VB6 code for an entire function
     * @param function IR function to generate code from
     * @param structuredCF Structured control flow tree (nullptr to generate from raw CFG)
     * @return VB6 source code as string
     */
    std::string generateFunction(const IRFunction& function, const StructuredNode* structuredCF = nullptr);
    
    /**
     * @brief Generate code for an expression
     * @param expr Expression to generate
     * @return VB6 expression string
     */
    std::string generateExpression(const IRExpression* expr);
    
    /**
     * @brief Generate code for a statement
     * @param stmt Statement to generate
     * @return VB6 statement string
     */
    std::string generateStatement(const IRStatement* stmt);

private:
    // Function generation
    std::string generateFunctionHeader(const IRFunction& function);
    std::string generateFunctionBody(const IRFunction& function, const StructuredNode* structuredCF);
    std::string generateLocalVariables(const IRFunction& function);
    
    // Structured control flow generation
    std::string generateStructuredNode(const StructuredNode* node);
    std::string generateSequence(const StructuredNode* node);
    std::string generateIfThen(const StructuredNode* node);
    std::string generateIfThenElse(const StructuredNode* node);
    std::string generateWhile(const StructuredNode* node);
    std::string generateDoWhile(const StructuredNode* node);
    std::string generateDoUntil(const StructuredNode* node);
    std::string generateGotoLabel(const StructuredNode* node);
    
    // Basic block generation (fallback for unstructured CFG)
    std::string generateBasicBlock(const IRBasicBlock* block, bool skipStructuralJumps = false);
    
    // Expression generation helpers
    std::string generateConstant(const IRConstant* constant);
    std::string generateVariable(const IRVariable* variable);
    std::string generateBinaryOp(IRExpressionKind op, const IRExpression* left, const IRExpression* right);
    std::string generateUnaryOp(IRExpressionKind op, const IRExpression* operand);
    std::string generateCall(const IRExpression* expr);
    std::string generateMemberAccess(const IRExpression* expr);
    std::string generateArrayIndex(const IRExpression* expr);
    std::string generateCast(const IRExpression* expr);
    
    // Operator helpers
    std::string getBinaryOperator(IRExpressionKind op) const;
    std::string getUnaryOperator(IRExpressionKind op) const;
    int getOperatorPrecedence(IRExpressionKind op) const;
    bool needsParentheses(IRExpressionKind parentOp, const IRExpression* childExpr, bool isLeftChild) const;
    
    // Type formatting
    std::string formatType(const IRType& type) const;
    std::string formatTypeDeclaration(const IRVariable& variable) const;
    
    // Indentation helpers
    void increaseIndent() { indentLevel_++; }
    void decreaseIndent() { if (indentLevel_ > 0) indentLevel_--; }
    std::string getIndent() const;
    std::string indent(const std::string& line) const;
    
    // Line formatting helpers
    std::string formatLine(const std::string& code) const;
    std::string splitLongLine(const std::string& line, size_t maxLength = 80) const;
    
    // State
    const TypeRecovery& typeRecovery_;
    int indentLevel_;
};

} // namespace VBDecompiler
