// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#pragma once

#include "../ir/IRFunction.h"
#include "TypeRecovery.h"
#include "ControlFlowStructurer.h"
#include "VBCodeGenerator.h"
#include <string>
#include <memory>

namespace VBDecompiler {

/**
 * @brief Main Decompiler Orchestrator
 * 
 * Coordinates the entire decompilation pipeline:
 * 1. Type Recovery - Infer VB types from IR
 * 2. Control Flow Structuring - Convert CFG to high-level control structures
 * 3. VB6 Code Generation - Generate readable VB6 source code
 * 
 * This is the main entry point for decompiling IR to VB6 source.
 */
class Decompiler {
public:
    Decompiler() = default;
    
    /**
     * @brief Decompile an IR function to VB6 source code
     * @param function IR function to decompile
     * @return VB6 source code as string
     */
    std::string decompile(IRFunction& function);
    
    /**
     * @brief Decompile an IR function with options
     * @param function IR function to decompile
     * @param structureControlFlow If true, attempt to structure control flow
     * @return VB6 source code as string
     */
    std::string decompile(IRFunction& function, bool structureControlFlow);
    
    /**
     * @brief Get the type recovery engine (for debugging/inspection)
     */
    const TypeRecovery& getTypeRecovery() const { return typeRecovery_; }

private:
    TypeRecovery typeRecovery_;
    ControlFlowStructurer structurer_;
};

} // namespace VBDecompiler
