// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "VBCodeGenerator.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace VBDecompiler {

// ============================================================================
// Function Generation
// ============================================================================

std::string VBCodeGenerator::generateFunction(const IRFunction& function, const StructuredNode* structuredCF) {
    std::ostringstream oss;
    
    // Function header
    oss << generateFunctionHeader(function) << "\n";
    
    indentLevel_ = 1;
    
    // Local variable declarations
    std::string localVars = generateLocalVariables(function);
    if (!localVars.empty()) {
        oss << localVars << "\n";
    }
    
    // Function body
    oss << generateFunctionBody(function, structuredCF);
    
    // Function footer
    indentLevel_ = 0;
    bool isSub = function.getReturnType().getKind() == VBTypeKind::VOID;
    oss << "End " << (isSub ? "Sub" : "Function") << "\n";
    
    return oss.str();
}

std::string VBCodeGenerator::generateFunctionHeader(const IRFunction& function) {
    std::ostringstream oss;
    
    bool isSub = function.getReturnType().getKind() == VBTypeKind::VOID;
    
    // Function or Sub keyword
    oss << (isSub ? "Sub " : "Function ") << function.getName();
    
    // Parameters
    oss << "(";
    const auto& params = function.getParameters();
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) oss << ", ";
        oss << params[i].getName() << " As " << formatType(params[i].getType());
    }
    oss << ")";
    
    // Return type for Function
    if (!isSub) {
        oss << " As " << formatType(function.getReturnType());
    }
    
    return oss.str();
}

std::string VBCodeGenerator::generateLocalVariables(const IRFunction& function) {
    std::ostringstream oss;
    
    const auto& locals = function.getLocalVariables();
    if (locals.empty()) {
        return "";
    }
    
    for (const auto& var : locals) {
        oss << indent("Dim " + formatTypeDeclaration(var)) << "\n";
    }
    
    return oss.str();
}

std::string VBCodeGenerator::generateFunctionBody(const IRFunction& function, const StructuredNode* structuredCF) {
    std::ostringstream oss;
    
    if (structuredCF != nullptr) {
        // Generate from structured control flow
        oss << generateStructuredNode(structuredCF);
    } else {
        // Fallback: Generate from CFG (emit blocks in order)
        const auto* entryBlock = function.getEntryBlock();
        if (entryBlock != nullptr) {
            // For now, just emit the entry block
            // A proper implementation would traverse the CFG in a sensible order
            oss << generateBasicBlock(entryBlock);
        }
    }
    
    return oss.str();
}

// ============================================================================
// Structured Control Flow Generation
// ============================================================================

std::string VBCodeGenerator::generateStructuredNode(const StructuredNode* node) {
    if (node == nullptr) {
        return "";
    }
    
    switch (node->getKind()) {
        case StructuredNodeKind::SEQUENCE:
            return generateSequence(node);
        case StructuredNodeKind::IF_THEN:
            return generateIfThen(node);
        case StructuredNodeKind::IF_THEN_ELSE:
            return generateIfThenElse(node);
        case StructuredNodeKind::WHILE:
            return generateWhile(node);
        case StructuredNodeKind::DO_WHILE:
            return generateDoWhile(node);
        case StructuredNodeKind::DO_UNTIL:
            return generateDoUntil(node);
        case StructuredNodeKind::GOTO_LABEL:
            return generateGotoLabel(node);
        default:
            return indent("' Unsupported control structure\n");
    }
}

std::string VBCodeGenerator::generateSequence(const StructuredNode* node) {
    std::ostringstream oss;
    
    // Generate code for blocks in this sequence
    for (const auto* block : node->getBlocks()) {
        oss << generateBasicBlock(block);
    }
    
    // Generate code for child nodes
    for (const auto& child : node->getChildren()) {
        oss << generateStructuredNode(child.get());
    }
    
    return oss.str();
}

std::string VBCodeGenerator::generateIfThen(const StructuredNode* node) {
    std::ostringstream oss;
    
    const auto* condition = node->getCondition();
    if (condition == nullptr) {
        return indent("' Error: If without condition\n");
    }
    
    // If...Then header
    oss << indent("If " + generateExpression(condition) + " Then\n");
    
    increaseIndent();
    
    // Then body
    for (const auto& child : node->getChildren()) {
        oss << generateStructuredNode(child.get());
    }
    
    decreaseIndent();
    
    // End If
    oss << indent("End If\n");
    
    return oss.str();
}

std::string VBCodeGenerator::generateIfThenElse(const StructuredNode* node) {
    std::ostringstream oss;
    
    const auto* condition = node->getCondition();
    if (condition == nullptr) {
        return indent("' Error: If without condition\n");
    }
    
    // If...Then header
    oss << indent("If " + generateExpression(condition) + " Then\n");
    
    increaseIndent();
    
    // Then body (first child)
    const auto& children = node->getChildren();
    if (!children.empty()) {
        oss << generateStructuredNode(children[0].get());
    }
    
    decreaseIndent();
    
    // Else body (second child)
    if (children.size() > 1) {
        oss << indent("Else\n");
        increaseIndent();
        oss << generateStructuredNode(children[1].get());
        decreaseIndent();
    }
    
    // End If
    oss << indent("End If\n");
    
    return oss.str();
}

std::string VBCodeGenerator::generateWhile(const StructuredNode* node) {
    std::ostringstream oss;
    
    const auto* condition = node->getCondition();
    if (condition == nullptr) {
        return indent("' Error: While without condition\n");
    }
    
    // While header
    oss << indent("While " + generateExpression(condition) + "\n");
    
    increaseIndent();
    
    // Loop body
    for (const auto& child : node->getChildren()) {
        oss << generateStructuredNode(child.get());
    }
    
    decreaseIndent();
    
    // Wend
    oss << indent("Wend\n");
    
    return oss.str();
}

std::string VBCodeGenerator::generateDoWhile(const StructuredNode* node) {
    std::ostringstream oss;
    
    const auto* condition = node->getCondition();
    
    // Do header
    oss << indent("Do\n");
    
    increaseIndent();
    
    // Loop body
    for (const auto& child : node->getChildren()) {
        oss << generateStructuredNode(child.get());
    }
    
    decreaseIndent();
    
    // Loop While footer
    if (condition != nullptr) {
        oss << indent("Loop While " + generateExpression(condition) + "\n");
    } else {
        oss << indent("Loop\n");
    }
    
    return oss.str();
}

std::string VBCodeGenerator::generateDoUntil(const StructuredNode* node) {
    std::ostringstream oss;
    
    const auto* condition = node->getCondition();
    
    // Do header
    oss << indent("Do\n");
    
    increaseIndent();
    
    // Loop body
    for (const auto& child : node->getChildren()) {
        oss << generateStructuredNode(child.get());
    }
    
    decreaseIndent();
    
    // Loop Until footer
    if (condition != nullptr) {
        oss << indent("Loop Until " + generateExpression(condition) + "\n");
    } else {
        oss << indent("Loop\n");
    }
    
    return oss.str();
}

std::string VBCodeGenerator::generateGotoLabel(const StructuredNode* node) {
    std::ostringstream oss;
    
    // Generate blocks with explicit labels and gotos
    for (const auto* block : node->getBlocks()) {
        // Label for this block
        oss << indent("Label_" + std::to_string(block->getId()) + ":\n");
        oss << generateBasicBlock(block);
    }
    
    return oss.str();
}

// ============================================================================
// Basic Block Generation
// ============================================================================

std::string VBCodeGenerator::generateBasicBlock(const IRBasicBlock* block) {
    std::ostringstream oss;
    
    if (block == nullptr) {
        return "";
    }
    
    for (const auto& stmt : block->getStatements()) {
        std::string stmtCode = generateStatement(stmt.get());
        if (!stmtCode.empty()) {
            oss << indent(stmtCode) << "\n";
        }
    }
    
    return oss.str();
}

// ============================================================================
// Statement Generation
// ============================================================================

std::string VBCodeGenerator::generateStatement(const IRStatement* stmt) {
    if (stmt == nullptr) {
        return "";
    }
    
    switch (stmt->getKind()) {
        case IRStatementKind::ASSIGN: {
            const auto* target = stmt->getTarget();
            const auto* value = stmt->getValue();
            if (target && value) {
                return target->getName() + " = " + generateExpression(value);
            }
            return "";
        }
        
        case IRStatementKind::STORE: {
            const auto* addr = stmt->getAddress();
            const auto* value = stmt->getStoreValue();
            if (addr && value) {
                return generateExpression(addr) + " = " + generateExpression(value);
            }
            return "";
        }
        
        case IRStatementKind::CALL: {
            const auto& funcName = stmt->getFunctionName();
            const auto& args = stmt->getArguments();
            
            std::ostringstream oss;
            oss << funcName;
            
            if (!args.empty()) {
                oss << " ";
                for (size_t i = 0; i < args.size(); i++) {
                    if (i > 0) oss << ", ";
                    oss << generateExpression(args[i].get());
                }
            }
            
            return oss.str();
        }
        
        case IRStatementKind::RETURN: {
            const auto* retVal = stmt->getReturnValue();
            if (retVal != nullptr) {
                return "Return " + generateExpression(retVal);
            }
            return "Return";
        }
        
        case IRStatementKind::BRANCH: {
            const auto* cond = stmt->getCondition();
            uint32_t target = stmt->getTargetBlockId();
            if (cond) {
                return "If " + generateExpression(cond) + " Then GoTo Label_" + std::to_string(target);
            }
            return "";
        }
        
        case IRStatementKind::GOTO: {
            uint32_t target = stmt->getGotoTarget();
            return "GoTo Label_" + std::to_string(target);
        }
        
        case IRStatementKind::LABEL: {
            uint32_t labelId = stmt->getLabelId();
            return "Label_" + std::to_string(labelId) + ":";
        }
        
        case IRStatementKind::NOP:
            return "' NOP";
        
        default:
            return "' Unknown statement";
    }
}

// ============================================================================
// Expression Generation
// ============================================================================

std::string VBCodeGenerator::generateExpression(const IRExpression* expr) {
    if (expr == nullptr) {
        return "";
    }
    
    switch (expr->getKind()) {
        case IRExpressionKind::CONSTANT:
            return generateConstant(expr->getConstant());
        
        case IRExpressionKind::VARIABLE:
        case IRExpressionKind::TEMPORARY:
            return generateVariable(expr->getVariable());
        
        case IRExpressionKind::NEGATE:
        case IRExpressionKind::NOT:
            return generateUnaryOp(expr->getKind(), expr->getOperand());
        
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
            return generateBinaryOp(expr->getKind(), expr->getLeft(), expr->getRight());
        
        case IRExpressionKind::CALL:
            return generateCall(expr);
        
        case IRExpressionKind::MEMBER_ACCESS:
            return generateMemberAccess(expr);
        
        case IRExpressionKind::ARRAY_INDEX:
            return generateArrayIndex(expr);
        
        case IRExpressionKind::CAST:
            return generateCast(expr);
        
        default:
            return "'UnknownExpr'";
    }
}

std::string VBCodeGenerator::generateConstant(const IRConstant* constant) {
    if (constant == nullptr) {
        return "";
    }
    
    const auto& value = constant->getValue();
    
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << std::get<double>(value);
        return oss.str();
    } else if (std::holds_alternative<std::string>(value)) {
        // Escape quotes in string
        std::string str = std::get<std::string>(value);
        std::string escaped;
        for (char c : str) {
            if (c == '"') {
                escaped += "\"\"";  // VB6 escapes quotes by doubling them
            } else {
                escaped += c;
            }
        }
        return "\"" + escaped + "\"";
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "True" : "False";
    }
    
    return "";
}

std::string VBCodeGenerator::generateVariable(const IRVariable* variable) {
    if (variable == nullptr) {
        return "";
    }
    return variable->getName();
}

std::string VBCodeGenerator::generateBinaryOp(IRExpressionKind op, const IRExpression* left, const IRExpression* right) {
    if (left == nullptr || right == nullptr) {
        return "";
    }
    
    std::string leftStr = generateExpression(left);
    std::string rightStr = generateExpression(right);
    std::string opStr = getBinaryOperator(op);
    
    // Add parentheses if needed based on precedence
    if (needsParentheses(op, left, true)) {
        leftStr = "(" + leftStr + ")";
    }
    if (needsParentheses(op, right, false)) {
        rightStr = "(" + rightStr + ")";
    }
    
    return leftStr + " " + opStr + " " + rightStr;
}

std::string VBCodeGenerator::generateUnaryOp(IRExpressionKind op, const IRExpression* operand) {
    if (operand == nullptr) {
        return "";
    }
    
    std::string operandStr = generateExpression(operand);
    std::string opStr = getUnaryOperator(op);
    
    // Add parentheses for complex expressions
    if (operand->getKind() != IRExpressionKind::CONSTANT &&
        operand->getKind() != IRExpressionKind::VARIABLE &&
        operand->getKind() != IRExpressionKind::TEMPORARY) {
        operandStr = "(" + operandStr + ")";
    }
    
    return opStr + operandStr;
}

std::string VBCodeGenerator::generateCall(const IRExpression* expr) {
    if (expr == nullptr) {
        return "";
    }
    
    std::ostringstream oss;
    oss << expr->getFunctionName() << "(";
    
    const auto& args = expr->getArguments();
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) oss << ", ";
        oss << generateExpression(args[i].get());
    }
    
    oss << ")";
    return oss.str();
}

std::string VBCodeGenerator::generateMemberAccess(const IRExpression* expr) {
    if (expr == nullptr) {
        return "";
    }
    
    const auto* object = expr->getObject();
    if (object == nullptr) {
        return "";
    }
    
    return generateExpression(object) + "." + expr->getMemberName();
}

std::string VBCodeGenerator::generateArrayIndex(const IRExpression* expr) {
    if (expr == nullptr) {
        return "";
    }
    
    const auto* array = expr->getArray();
    if (array == nullptr) {
        return "";
    }
    
    std::ostringstream oss;
    oss << generateExpression(array) << "(";
    
    const auto& indices = expr->getIndices();
    for (size_t i = 0; i < indices.size(); i++) {
        if (i > 0) oss << ", ";
        oss << generateExpression(indices[i].get());
    }
    
    oss << ")";
    return oss.str();
}

std::string VBCodeGenerator::generateCast(const IRExpression* expr) {
    if (expr == nullptr) {
        return "";
    }
    
    const auto* operand = expr->getOperand();
    if (operand == nullptr) {
        return "";
    }
    
    const auto& targetType = expr->getType();
    std::string typeName = formatType(targetType);
    
    // VB6 type conversion functions
    if (targetType.getKind() == VBTypeKind::INTEGER) {
        return "CInt(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::LONG) {
        return "CLng(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::SINGLE) {
        return "CSng(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::DOUBLE) {
        return "CDbl(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::STRING) {
        return "CStr(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::BYTE) {
        return "CByte(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::BOOLEAN) {
        return "CBool(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::DATE) {
        return "CDate(" + generateExpression(operand) + ")";
    } else if (targetType.getKind() == VBTypeKind::VARIANT) {
        return "CVar(" + generateExpression(operand) + ")";
    }
    
    return generateExpression(operand);
}

// ============================================================================
// Operator Helpers
// ============================================================================

std::string VBCodeGenerator::getBinaryOperator(IRExpressionKind op) const {
    switch (op) {
        case IRExpressionKind::ADD:           return "+";
        case IRExpressionKind::SUBTRACT:      return "-";
        case IRExpressionKind::MULTIPLY:      return "*";
        case IRExpressionKind::DIVIDE:        return "/";
        case IRExpressionKind::INT_DIVIDE:    return "\\";
        case IRExpressionKind::MODULO:        return "Mod";
        case IRExpressionKind::EQUAL:         return "=";
        case IRExpressionKind::NOT_EQUAL:     return "<>";
        case IRExpressionKind::LESS_THAN:     return "<";
        case IRExpressionKind::LESS_EQUAL:    return "<=";
        case IRExpressionKind::GREATER_THAN:  return ">";
        case IRExpressionKind::GREATER_EQUAL: return ">=";
        case IRExpressionKind::AND:           return "And";
        case IRExpressionKind::OR:            return "Or";
        case IRExpressionKind::XOR:           return "Xor";
        case IRExpressionKind::CONCATENATE:   return "&";
        default:                              return "?";
    }
}

std::string VBCodeGenerator::getUnaryOperator(IRExpressionKind op) const {
    switch (op) {
        case IRExpressionKind::NEGATE: return "-";
        case IRExpressionKind::NOT:    return "Not ";
        default:                       return "?";
    }
}

int VBCodeGenerator::getOperatorPrecedence(IRExpressionKind op) const {
    switch (op) {
        case IRExpressionKind::NEGATE:
        case IRExpressionKind::NOT:
            return 10;  // Unary operators
        
        case IRExpressionKind::MULTIPLY:
        case IRExpressionKind::DIVIDE:
        case IRExpressionKind::INT_DIVIDE:
            return 9;
        
        case IRExpressionKind::MODULO:
            return 8;
        
        case IRExpressionKind::ADD:
        case IRExpressionKind::SUBTRACT:
            return 7;
        
        case IRExpressionKind::CONCATENATE:
            return 6;
        
        case IRExpressionKind::EQUAL:
        case IRExpressionKind::NOT_EQUAL:
        case IRExpressionKind::LESS_THAN:
        case IRExpressionKind::LESS_EQUAL:
        case IRExpressionKind::GREATER_THAN:
        case IRExpressionKind::GREATER_EQUAL:
            return 5;
        
        case IRExpressionKind::AND:
            return 4;
        
        case IRExpressionKind::OR:
            return 3;
        
        case IRExpressionKind::XOR:
            return 2;
        
        default:
            return 0;
    }
}

bool VBCodeGenerator::needsParentheses(IRExpressionKind parentOp, const IRExpression* childExpr, bool isLeftChild) const {
    if (childExpr == nullptr) {
        return false;
    }
    
    // Constants and variables never need parentheses
    IRExpressionKind childOp = childExpr->getKind();
    if (childOp == IRExpressionKind::CONSTANT ||
        childOp == IRExpressionKind::VARIABLE ||
        childOp == IRExpressionKind::TEMPORARY) {
        return false;
    }
    
    // Compare precedence
    int parentPrecedence = getOperatorPrecedence(parentOp);
    int childPrecedence = getOperatorPrecedence(childOp);
    
    // Lower precedence needs parentheses
    if (childPrecedence < parentPrecedence) {
        return true;
    }
    
    // Same precedence: right-associative operators need parentheses on right side
    if (childPrecedence == parentPrecedence && !isLeftChild) {
        return true;
    }
    
    return false;
}

// ============================================================================
// Type Formatting
// ============================================================================

std::string VBCodeGenerator::formatType(const IRType& type) const {
    switch (type.getKind()) {
        case VBTypeKind::VOID:         return "Void";
        case VBTypeKind::BYTE:         return "Byte";
        case VBTypeKind::BOOLEAN:      return "Boolean";
        case VBTypeKind::INTEGER:      return "Integer";
        case VBTypeKind::LONG:         return "Long";
        case VBTypeKind::SINGLE:       return "Single";
        case VBTypeKind::DOUBLE:       return "Double";
        case VBTypeKind::CURRENCY:     return "Currency";
        case VBTypeKind::DATE:         return "Date";
        case VBTypeKind::STRING:       return "String";
        case VBTypeKind::OBJECT:       return "Object";
        case VBTypeKind::VARIANT:      return "Variant";
        case VBTypeKind::USER_DEFINED: return type.getTypeName();
        case VBTypeKind::ARRAY: {
            if (type.getElementType() != nullptr) {
                return formatType(*type.getElementType()) + "()";
            }
            return "Array";
        }
        case VBTypeKind::UNKNOWN:      return "Variant";  // Default to Variant for unknown
        default:                       return "Variant";
    }
}

std::string VBCodeGenerator::formatTypeDeclaration(const IRVariable& variable) const {
    return variable.getName() + " As " + formatType(variable.getType());
}

// ============================================================================
// Indentation Helpers
// ============================================================================

std::string VBCodeGenerator::getIndent() const {
    return std::string(indentLevel_ * 4, ' ');  // 4 spaces per indent level
}

std::string VBCodeGenerator::indent(const std::string& line) const {
    if (line.empty()) {
        return line;
    }
    return getIndent() + line;
}

std::string VBCodeGenerator::formatLine(const std::string& code) const {
    return indent(code);
}

std::string VBCodeGenerator::splitLongLine(const std::string& line, size_t maxLength) const {
    // For now, don't split lines - just return as-is
    // A proper implementation would split at appropriate places and use " _" continuation
    return line;
}

} // namespace VBDecompiler
