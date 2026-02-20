// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#ifndef IRSTATEMENT_H
#define IRSTATEMENT_H

#include "IRExpression.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace VBDecompiler {

/**
 * @brief Statement Kind - Types of IR statements
 */
enum class IRStatementKind {
    ASSIGN,         // variable = expression
    STORE,          // [address] = expression
    CALL,           // Call subroutine (no return value)
    RETURN,         // Return [expression]
    BRANCH,         // Conditional branch (If condition Then goto target)
    GOTO,           // Unconditional jump (goto target)
    LABEL,          // Label marker for jump targets
    NOP             // No operation (placeholder)
};

// Forward declaration
class IRBasicBlock;

/**
 * @brief IR Statement - Represents a statement in the IR
 * 
 * Statements represent side-effecting operations:
 * - Assignments (variable = expression)
 * - Memory stores ([address] = value)
 * - Function/subroutine calls
 * - Control flow (return, branch, goto)
 */
class IRStatement {
public:
    /**
     * @brief Create an assignment statement: variable = expression
     */
    static std::unique_ptr<IRStatement> makeAssign(
        IRVariable target,
        std::unique_ptr<IRExpression> value
    );
    
    /**
     * @brief Create a store statement: [address] = value
     */
    static std::unique_ptr<IRStatement> makeStore(
        std::unique_ptr<IRExpression> address,
        std::unique_ptr<IRExpression> value
    );
    
    /**
     * @brief Create a call statement (subroutine call, no return value)
     */
    static std::unique_ptr<IRStatement> makeCall(
        std::string functionName,
        std::vector<std::unique_ptr<IRExpression>> arguments
    );
    
    /**
     * @brief Create a return statement
     * @param value Optional return value (nullptr for Sub/procedures)
     */
    static std::unique_ptr<IRStatement> makeReturn(
        std::unique_ptr<IRExpression> value = nullptr
    );
    
    /**
     * @brief Create a conditional branch: If condition Then goto target
     */
    static std::unique_ptr<IRStatement> makeBranch(
        std::unique_ptr<IRExpression> condition,
        uint32_t targetBlockId
    );
    
    /**
     * @brief Create an unconditional goto
     */
    static std::unique_ptr<IRStatement> makeGoto(uint32_t targetBlockId);
    
    /**
     * @brief Create a label marker
     */
    static std::unique_ptr<IRStatement> makeLabel(uint32_t labelId);
    
    /**
     * @brief Create a NOP (no operation)
     */
    static std::unique_ptr<IRStatement> makeNop();
    
    // Getters
    [[nodiscard]] IRStatementKind getKind() const { return kind_; }
    
    // For ASSIGN statements
    [[nodiscard]] const IRVariable* getTarget() const;
    [[nodiscard]] const IRExpression* getValue() const;
    
    // For STORE statements
    [[nodiscard]] const IRExpression* getAddress() const;
    [[nodiscard]] const IRExpression* getStoreValue() const;
    
    // For CALL statements
    [[nodiscard]] const std::string& getFunctionName() const;
    [[nodiscard]] const std::vector<std::unique_ptr<IRExpression>>& getArguments() const;
    
    // For RETURN statements
    [[nodiscard]] const IRExpression* getReturnValue() const;
    
    // For BRANCH statements
    [[nodiscard]] const IRExpression* getCondition() const;
    [[nodiscard]] uint32_t getTargetBlockId() const;
    
    // For GOTO statements
    [[nodiscard]] uint32_t getGotoTarget() const;
    
    // For LABEL statements
    [[nodiscard]] uint32_t getLabelId() const;
    
    /**
     * @brief Convert statement to string representation
     */
    [[nodiscard]] std::string toString() const;
    
    // Disable copying
    IRStatement(const IRStatement&) = delete;
    IRStatement& operator=(const IRStatement&) = delete;
    
    // Enable moving
    IRStatement(IRStatement&&) noexcept = default;
    IRStatement& operator=(IRStatement&&) noexcept = default;
    
    ~IRStatement() = default;

private:
    explicit IRStatement(IRStatementKind kind) : kind_(kind) {}
    
    IRStatementKind kind_;
    
    // Data for different statement types
    struct Data {
        // For ASSIGN
        std::unique_ptr<IRVariable> target;
        std::unique_ptr<IRExpression> value;
        
        // For STORE
        std::unique_ptr<IRExpression> address;
        std::unique_ptr<IRExpression> storeValue;
        
        // For CALL
        std::string functionName;
        std::vector<std::unique_ptr<IRExpression>> arguments;
        
        // For RETURN
        std::unique_ptr<IRExpression> returnValue;
        
        // For BRANCH
        std::unique_ptr<IRExpression> condition;
        uint32_t targetBlockId = 0;
        
        // For GOTO
        uint32_t gotoTarget = 0;
        
        // For LABEL
        uint32_t labelId = 0;
        
        Data() = default;
        ~Data() = default;
        
        // Make movable
        Data(Data&&) noexcept = default;
        Data& operator=(Data&&) noexcept = default;
        
        // Delete copy
        Data(const Data&) = delete;
        Data& operator=(const Data&) = delete;
    } data_;
};

} // namespace VBDecompiler

#endif // IRSTATEMENT_H
