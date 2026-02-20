// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "Decompiler.h"

namespace VBDecompiler {

std::string Decompiler::decompile(IRFunction& function) {
    return decompile(function, true);
}

std::string Decompiler::decompile(IRFunction& function, bool structureControlFlow) {
    // Step 1: Type Recovery
    // Analyze the function and infer types for all variables and expressions
    typeRecovery_.clear();
    typeRecovery_.analyzeFunction(function);
    
    // Step 2: Control Flow Structuring (optional)
    std::unique_ptr<StructuredNode> structuredCF = nullptr;
    if (structureControlFlow) {
        structuredCF = structurer_.structureFunction(function);
    }
    
    // Step 3: VB6 Code Generation
    VBCodeGenerator generator(typeRecovery_);
    return generator.generateFunction(function, structuredCF.get());
}

} // namespace VBDecompiler
