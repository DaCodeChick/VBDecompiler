// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#ifndef IREXPRESSION_H
#define IREXPRESSION_H

#include "IRType.h"
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <cstdint>

namespace VBDecompiler {

/**
 * @brief Expression Kind - Types of IR expressions
 */
enum class IRExpressionKind {
    // Literals
    CONSTANT,           // Constant value (integer, float, string)
    
    // Variables
    VARIABLE,           // Local variable or parameter
    TEMPORARY,          // SSA temporary variable
    
    // Unary operations
    NEGATE,             // -x
    NOT,                // Not x
    
    // Binary operations - Arithmetic
    ADD,                // x + y
    SUBTRACT,           // x - y
    MULTIPLY,           // x * y
    DIVIDE,             // x / y
    INT_DIVIDE,         // x \ y (integer division)
    MODULO,             // x Mod y
    
    // Binary operations - Comparison
    EQUAL,              // x = y
    NOT_EQUAL,          // x <> y
    LESS_THAN,          // x < y
    LESS_EQUAL,         // x <= y
    GREATER_THAN,       // x > y
    GREATER_EQUAL,      // x >= y
    
    // Binary operations - Logical
    AND,                // x And y
    OR,                 // x Or y
    XOR,                // x Xor y
    
    // Binary operations - String
    CONCATENATE,        // x & y
    
    // Memory operations
    LOAD,               // Load from memory [address]
    MEMBER_ACCESS,      // obj.member or record.field
    ARRAY_INDEX,        // arr(index)
    
    // Function call
    CALL,               // function(args...)
    
    // Type conversion
    CAST                // CType(x, Type)
};

/**
 * @brief Constant Value - Represents a constant value in an expression
 */
class IRConstant {
public:
    using ValueType = std::variant<
        int64_t,        // Integer constant
        double,         // Floating point constant
        std::string,    // String constant
        bool            // Boolean constant
    >;
    
    explicit IRConstant(int64_t value) : value_(value), type_(IRTypes::Long) {}
    explicit IRConstant(double value) : value_(value), type_(IRTypes::Double) {}
    explicit IRConstant(const std::string& value) : value_(value), type_(IRTypes::String) {}
    explicit IRConstant(const char* value) : value_(std::string(value)), type_(IRTypes::String) {}
    explicit IRConstant(bool value) : value_(value), type_(IRTypes::Boolean) {}
    
    [[nodiscard]] const ValueType& getValue() const { return value_; }
    [[nodiscard]] const IRType& getType() const { return type_; }
    
    [[nodiscard]] std::string toString() const;
    
private:
    ValueType value_;
    IRType type_;
};

/**
 * @brief IR Variable - Represents a variable reference
 */
class IRVariable {
public:
    explicit IRVariable(uint32_t id, std::string name, IRType type)
        : id_(id), name_(std::move(name)), type_(std::move(type)) {}
    
    [[nodiscard]] uint32_t getId() const { return id_; }
    [[nodiscard]] const std::string& getName() const { return name_; }
    [[nodiscard]] const IRType& getType() const { return type_; }
    
    void setType(const IRType& type) { type_ = type; }
    
    [[nodiscard]] std::string toString() const { return name_; }
    
private:
    uint32_t id_;           // Unique variable ID
    std::string name_;      // Variable name (v0, v1, etc. for SSA)
    IRType type_;           // Variable type
};

// Forward declaration
class IRExpression;

/**
 * @brief IR Expression - Represents an expression in the IR
 * 
 * Expressions are immutable and can represent:
 * - Constants (literals)
 * - Variables
 * - Unary operations (negation, logical NOT)
 * - Binary operations (arithmetic, comparison, logical)
 * - Function calls
 * - Memory accesses
 */
class IRExpression {
public:
    /**
     * @brief Create a constant expression
     */
    static std::unique_ptr<IRExpression> makeConstant(const IRConstant& constant);
    
    /**
     * @brief Create a variable expression
     */
    static std::unique_ptr<IRExpression> makeVariable(const IRVariable& variable);
    
    /**
     * @brief Create a unary operation expression
     */
    static std::unique_ptr<IRExpression> makeUnary(
        IRExpressionKind op,
        std::unique_ptr<IRExpression> operand,
        const IRType& resultType
    );
    
    /**
     * @brief Create a binary operation expression
     */
    static std::unique_ptr<IRExpression> makeBinary(
        IRExpressionKind op,
        std::unique_ptr<IRExpression> left,
        std::unique_ptr<IRExpression> right,
        const IRType& resultType
    );
    
    /**
     * @brief Create a function call expression
     */
    static std::unique_ptr<IRExpression> makeCall(
        std::string functionName,
        std::vector<std::unique_ptr<IRExpression>> arguments,
        const IRType& resultType
    );
    
    /**
     * @brief Create a member access expression (obj.member)
     */
    static std::unique_ptr<IRExpression> makeMemberAccess(
        std::unique_ptr<IRExpression> object,
        std::string memberName,
        const IRType& resultType
    );
    
    /**
     * @brief Create an array indexing expression
     */
    static std::unique_ptr<IRExpression> makeArrayIndex(
        std::unique_ptr<IRExpression> array,
        std::vector<std::unique_ptr<IRExpression>> indices,
        const IRType& resultType
    );
    
    /**
     * @brief Create a type cast expression
     */
    static std::unique_ptr<IRExpression> makeCast(
        std::unique_ptr<IRExpression> operand,
        const IRType& targetType
    );
    
    // Getters
    [[nodiscard]] IRExpressionKind getKind() const { return kind_; }
    [[nodiscard]] const IRType& getType() const { return type_; }
    
    // Get constant value (only valid for CONSTANT kind)
    [[nodiscard]] const IRConstant* getConstant() const;
    
    // Get variable (only valid for VARIABLE/TEMPORARY kind)
    [[nodiscard]] const IRVariable* getVariable() const;
    
    // Get operands (for unary/binary operations)
    [[nodiscard]] const IRExpression* getOperand() const;  // For unary
    [[nodiscard]] const IRExpression* getLeft() const;     // For binary
    [[nodiscard]] const IRExpression* getRight() const;    // For binary
    
    // Get call information
    [[nodiscard]] const std::string& getFunctionName() const;
    [[nodiscard]] const std::vector<std::unique_ptr<IRExpression>>& getArguments() const;
    
    // Get member access information
    [[nodiscard]] const IRExpression* getObject() const;
    [[nodiscard]] const std::string& getMemberName() const;
    
    // Get array index information
    [[nodiscard]] const IRExpression* getArray() const;
    [[nodiscard]] const std::vector<std::unique_ptr<IRExpression>>& getIndices() const;
    
    /**
     * @brief Convert expression to string representation
     */
    [[nodiscard]] std::string toString() const;
    
    // Disable copying (use unique_ptr for ownership)
    IRExpression(const IRExpression&) = delete;
    IRExpression& operator=(const IRExpression&) = delete;
    
    // Enable moving
    IRExpression(IRExpression&&) noexcept = default;
    IRExpression& operator=(IRExpression&&) noexcept = default;
    
    ~IRExpression() = default;

private:
    explicit IRExpression(IRExpressionKind kind, IRType type)
        : kind_(kind), type_(std::move(type)) {}
    
    IRExpressionKind kind_;
    IRType type_;
    
    // Data union for different expression types
    struct Data {
        // For constants
        std::unique_ptr<IRConstant> constant;
        
        // For variables
        std::unique_ptr<IRVariable> variable;
        
        // For unary operations
        std::unique_ptr<IRExpression> operand;
        
        // For binary operations
        std::unique_ptr<IRExpression> left;
        std::unique_ptr<IRExpression> right;
        
        // For function calls
        std::string functionName;
        std::vector<std::unique_ptr<IRExpression>> arguments;
        
        // For member access
        std::unique_ptr<IRExpression> object;
        std::string memberName;
        
        // For array indexing
        std::unique_ptr<IRExpression> array;
        std::vector<std::unique_ptr<IRExpression>> indices;
        
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

#endif // IREXPRESSION_H
