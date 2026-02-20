// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "IRFunction.h"
#include <sstream>

namespace VBDecompiler {

// IRBasicBlock::toString()
std::string IRBasicBlock::toString() const {
    std::ostringstream oss;
    
    oss << "BB" << id_ << ":\n";
    
    // Show predecessors
    if (!predecessors_.empty()) {
        oss << "  ; predecessors: ";
        bool first = true;
        for (uint32_t predId : predecessors_) {
            if (!first) oss << ", ";
            oss << "BB" << predId;
            first = false;
        }
        oss << "\n";
    }
    
    // Show statements
    for (const auto& stmt : statements_) {
        oss << "  " << stmt->toString() << "\n";
    }
    
    // Show successors
    if (!successors_.empty()) {
        oss << "  ; successors: ";
        bool first = true;
        for (uint32_t succId : successors_) {
            if (!first) oss << ", ";
            oss << "BB" << succId;
            first = false;
        }
        oss << "\n";
    }
    
    return oss.str();
}

// IRFunction::toString()
std::string IRFunction::toString() const {
    std::ostringstream oss;
    
    // Function signature
    oss << "Function " << name_ << "(";
    for (size_t i = 0; i < parameters_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << parameters_[i].getName() << " As " << parameters_[i].getType().toString();
    }
    oss << ")";
    
    if (returnType_.getKind() != VBTypeKind::VOID) {
        oss << " As " << returnType_.toString();
    }
    oss << "\n";
    
    // Address
    if (address_ != 0) {
        oss << "  ; Address: 0x" << std::hex << address_ << std::dec << "\n";
    }
    
    // Local variables
    if (!localVariables_.empty()) {
        oss << "  ; Local variables:\n";
        for (const auto& var : localVariables_) {
            oss << "  ;   " << var.getName() << " As " 
                << var.getType().toString() << "\n";
        }
    }
    
    oss << "\n";
    
    // Basic blocks in order (entry block first, then others)
    if (const auto* entryBlock = getEntryBlock()) {
        oss << entryBlock->toString() << "\n";
    }
    
    for (const auto& [id, block] : blocks_) {
        if (id != entryBlockId_) {
            oss << block->toString() << "\n";
        }
    }
    
    oss << "End Function\n";
    
    return oss.str();
}

} // namespace VBDecompiler
