// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#ifndef IRTYPE_H
#define IRTYPE_H

#include <string>
#include <memory>
#include <cstdint>

namespace VBDecompiler {

/**
 * @brief VB Type Kind - Represents Visual Basic data types
 */
enum class VBTypeKind {
    VOID,           // No type (for procedures without return value)
    BYTE,           // 8-bit unsigned integer
    BOOLEAN,        // True/False
    INTEGER,        // 16-bit signed integer
    LONG,           // 32-bit signed integer
    SINGLE,         // 32-bit floating point
    DOUBLE,         // 64-bit floating point
    CURRENCY,       // Fixed-point currency type
    DATE,           // Date/time value
    STRING,         // Variable-length string
    OBJECT,         // Object reference
    VARIANT,        // Variant type (can hold any type)
    USER_DEFINED,   // User-defined type (UDT)
    ARRAY,          // Array type
    UNKNOWN         // Unknown/unresolved type
};

/**
 * @brief IR Type - Represents a type in the intermediate representation
 * 
 * This class represents VB types used during decompilation.
 * Supports basic types, arrays, and user-defined types.
 */
class IRType {
public:
    /**
     * @brief Create a basic VB type
     */
    explicit IRType(VBTypeKind kind) 
        : kind_(kind), size_(getSizeForKind(kind)), isArray_(false) {}
    
    /**
     * @brief Create an array type
     */
    static IRType makeArray(const IRType& elementType, int dimensions = 1) {
        IRType arrayType(VBTypeKind::ARRAY);
        arrayType.elementType_ = std::make_unique<IRType>(elementType);
        arrayType.arrayDimensions_ = dimensions;
        arrayType.isArray_ = true;
        return arrayType;
    }
    
    /**
     * @brief Create a user-defined type
     */
    static IRType makeUserDefined(const std::string& typeName) {
        IRType udt(VBTypeKind::USER_DEFINED);
        udt.typeName_ = typeName;
        return udt;
    }
    
    // Copy/Move constructors and assignment operators
    IRType(const IRType& other);
    IRType(IRType&& other) noexcept = default;
    IRType& operator=(const IRType& other);
    IRType& operator=(IRType&& other) noexcept = default;
    
    ~IRType() = default;
    
    // Getters
    [[nodiscard]] VBTypeKind getKind() const { return kind_; }
    [[nodiscard]] uint32_t getSize() const { return size_; }
    [[nodiscard]] bool isArray() const { return isArray_; }
    [[nodiscard]] int getArrayDimensions() const { return arrayDimensions_; }
    [[nodiscard]] const IRType* getElementType() const { return elementType_.get(); }
    [[nodiscard]] const std::string& getTypeName() const { return typeName_; }
    
    /**
     * @brief Get VB type name as string
     */
    [[nodiscard]] std::string toString() const;
    
    /**
     * @brief Check if this is a numeric type
     */
    [[nodiscard]] bool isNumeric() const {
        return kind_ == VBTypeKind::BYTE || kind_ == VBTypeKind::INTEGER || 
               kind_ == VBTypeKind::LONG || kind_ == VBTypeKind::SINGLE || 
               kind_ == VBTypeKind::DOUBLE || kind_ == VBTypeKind::CURRENCY;
    }
    
    /**
     * @brief Check if this is an integer type
     */
    [[nodiscard]] bool isInteger() const {
        return kind_ == VBTypeKind::BYTE || kind_ == VBTypeKind::INTEGER || 
               kind_ == VBTypeKind::LONG;
    }
    
    /**
     * @brief Check if this is a floating point type
     */
    [[nodiscard]] bool isFloatingPoint() const {
        return kind_ == VBTypeKind::SINGLE || kind_ == VBTypeKind::DOUBLE;
    }
    
    /**
     * @brief Check if this is a reference type (String, Object, Array)
     */
    [[nodiscard]] bool isReference() const {
        return kind_ == VBTypeKind::STRING || kind_ == VBTypeKind::OBJECT || 
               kind_ == VBTypeKind::ARRAY;
    }
    
    /**
     * @brief Equality comparison
     */
    bool operator==(const IRType& other) const;
    bool operator!=(const IRType& other) const { return !(*this == other); }

private:
    VBTypeKind kind_;
    uint32_t size_;              // Size in bytes
    bool isArray_;
    int arrayDimensions_;
    std::unique_ptr<IRType> elementType_;  // For array types
    std::string typeName_;       // For user-defined types
    
    /**
     * @brief Get default size for a VB type kind
     */
    static uint32_t getSizeForKind(VBTypeKind kind);
};

// Common type constants
namespace IRTypes {
    inline const IRType Void(VBTypeKind::VOID);
    inline const IRType Byte(VBTypeKind::BYTE);
    inline const IRType Boolean(VBTypeKind::BOOLEAN);
    inline const IRType Integer(VBTypeKind::INTEGER);
    inline const IRType Long(VBTypeKind::LONG);
    inline const IRType Single(VBTypeKind::SINGLE);
    inline const IRType Double(VBTypeKind::DOUBLE);
    inline const IRType Currency(VBTypeKind::CURRENCY);
    inline const IRType Date(VBTypeKind::DATE);
    inline const IRType String(VBTypeKind::STRING);
    inline const IRType Object(VBTypeKind::OBJECT);
    inline const IRType Variant(VBTypeKind::VARIANT);
    inline const IRType Unknown(VBTypeKind::UNKNOWN);
}

} // namespace VBDecompiler

#endif // IRTYPE_H
