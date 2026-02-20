// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "IRType.h"

namespace VBDecompiler {

// Copy constructor
IRType::IRType(const IRType& other)
    : kind_(other.kind_)
    , size_(other.size_)
    , isArray_(other.isArray_)
    , arrayDimensions_(other.arrayDimensions_)
    , elementType_(other.elementType_ ? std::make_unique<IRType>(*other.elementType_) : nullptr)
    , typeName_(other.typeName_)
{
}

// Copy assignment
IRType& IRType::operator=(const IRType& other) {
    if (this != &other) {
        kind_ = other.kind_;
        size_ = other.size_;
        isArray_ = other.isArray_;
        arrayDimensions_ = other.arrayDimensions_;
        elementType_ = other.elementType_ ? std::make_unique<IRType>(*other.elementType_) : nullptr;
        typeName_ = other.typeName_;
    }
    return *this;
}

// Get size for a VB type kind
uint32_t IRType::getSizeForKind(VBTypeKind kind) {
    switch (kind) {
        case VBTypeKind::VOID:        return 0;
        case VBTypeKind::BYTE:        return 1;
        case VBTypeKind::BOOLEAN:     return 2;  // VB Boolean is 2 bytes
        case VBTypeKind::INTEGER:     return 2;
        case VBTypeKind::LONG:        return 4;
        case VBTypeKind::SINGLE:      return 4;
        case VBTypeKind::DOUBLE:      return 8;
        case VBTypeKind::CURRENCY:    return 8;
        case VBTypeKind::DATE:        return 8;
        case VBTypeKind::STRING:      return 4;  // String pointer
        case VBTypeKind::OBJECT:      return 4;  // Object pointer
        case VBTypeKind::VARIANT:     return 16; // Variant structure
        case VBTypeKind::USER_DEFINED: return 0;  // Size depends on UDT
        case VBTypeKind::ARRAY:       return 4;  // Array descriptor pointer
        case VBTypeKind::UNKNOWN:     return 0;
        default:                      return 0;
    }
}

// Convert type to string
std::string IRType::toString() const {
    std::string result;
    
    switch (kind_) {
        case VBTypeKind::VOID:        result = "Void"; break;
        case VBTypeKind::BYTE:        result = "Byte"; break;
        case VBTypeKind::BOOLEAN:     result = "Boolean"; break;
        case VBTypeKind::INTEGER:     result = "Integer"; break;
        case VBTypeKind::LONG:        result = "Long"; break;
        case VBTypeKind::SINGLE:      result = "Single"; break;
        case VBTypeKind::DOUBLE:      result = "Double"; break;
        case VBTypeKind::CURRENCY:    result = "Currency"; break;
        case VBTypeKind::DATE:        result = "Date"; break;
        case VBTypeKind::STRING:      result = "String"; break;
        case VBTypeKind::OBJECT:      result = "Object"; break;
        case VBTypeKind::VARIANT:     result = "Variant"; break;
        case VBTypeKind::USER_DEFINED: result = typeName_; break;
        case VBTypeKind::ARRAY:
            if (elementType_) {
                result = elementType_->toString() + "(";
                for (int i = 0; i < arrayDimensions_; ++i) {
                    if (i > 0) result += ",";
                }
                result += ")";
            } else {
                result = "Array";
            }
            break;
        case VBTypeKind::UNKNOWN:     result = "?"; break;
        default:                      result = "Unknown"; break;
    }
    
    return result;
}

// Equality comparison
bool IRType::operator==(const IRType& other) const {
    if (kind_ != other.kind_) return false;
    if (isArray_ != other.isArray_) return false;
    
    // For arrays, compare element types and dimensions
    if (isArray_) {
        if (arrayDimensions_ != other.arrayDimensions_) return false;
        if (elementType_ && other.elementType_) {
            return *elementType_ == *other.elementType_;
        }
        return elementType_ == other.elementType_;  // Both null
    }
    
    // For user-defined types, compare names
    if (kind_ == VBTypeKind::USER_DEFINED) {
        return typeName_ == other.typeName_;
    }
    
    return true;
}

} // namespace VBDecompiler
