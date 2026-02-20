// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: MIT

#pragma once

#include "IRFunction.h"
#include "../disasm/pcode/PCodeInstruction.h"
#include <memory>
#include <vector>
#include <stack>
#include <map>
#include <string>

namespace VBDecompiler {

/**
 * @brief P-Code to IR Lifter
 * 
 * Converts Visual Basic P-Code instructions to IR (Intermediate Representation).
 * P-Code is a stack-based bytecode format, so this lifter maintains a virtual
 * evaluation stack and converts stack operations to SSA-style IR with variables.
 * 
 * Architecture:
 * - P-Code pushes/pops values on a virtual stack
 * - Lifter converts stack operations to temporary variables (t0, t1, t2, ...)
 * - Creates IRBasicBlocks with CFG edges for branches
 * - Maps P-Code types to VB types in IRType system
 */
class PCodeLifter {
public:
    PCodeLifter() = default;
    
    /**
     * @brief Lift a sequence of P-Code instructions to an IR function
     * @param instructions P-Code instructions to lift
     * @param functionName Name for the generated function
     * @param startAddress Starting address of the function
     * @return IRFunction with lifted IR, or nullptr on error
     */
    std::unique_ptr<IRFunction> lift(
        const std::vector<PCodeInstruction>& instructions,
        const std::string& functionName,
        uint32_t startAddress);
    
    /**
     * @brief Get last error message
     */
    const std::string& getLastError() const { return lastError_; }

private:
    // Context for lifting a single function
    struct LiftContext {
        std::unique_ptr<IRFunction> function;
        IRBasicBlock* currentBlock = nullptr;
        
        // Stack modeling (tracks expression IDs on virtual stack)
        std::vector<std::unique_ptr<IRExpression>> evalStack;
        
        // Variable generation
        size_t nextTempId = 0;
        size_t nextVarId = 0;
        
        // Label tracking for branches
        std::map<uint32_t, IRBasicBlock*> addressToBlock;
        std::vector<std::pair<IRBasicBlock*, uint32_t>> pendingBranches;  // (block, target address)
        
        // Helper to generate temporary variable
        std::unique_ptr<IRVariable> makeTempVar(const IRType& type) {
            return std::make_unique<IRVariable>(
                nextTempId++,
                "t" + std::to_string(nextTempId - 1),
                type
            );
        }
        
        // Helper to generate local variable
        std::unique_ptr<IRVariable> makeLocalVar(const std::string& name, const IRType& type) {
            return std::make_unique<IRVariable>(
                nextVarId++,
                name,
                type
            );
        }
        
        // Stack operations
        std::unique_ptr<IRExpression> popStack() {
            if (evalStack.empty()) {
                return nullptr;
            }
            auto expr = std::move(evalStack.back());
            evalStack.pop_back();
            return expr;
        }
        
        void pushStack(std::unique_ptr<IRExpression> expr) {
            evalStack.push_back(std::move(expr));
        }
    };
    
    // Instruction lifting methods
    bool liftInstruction(const PCodeInstruction& instr, LiftContext& ctx);
    
    // Category-specific lifters
    bool liftArithmetic(const PCodeInstruction& instr, LiftContext& ctx);
    bool liftComparison(const PCodeInstruction& instr, LiftContext& ctx);
    bool liftLogical(const PCodeInstruction& instr, LiftContext& ctx);
    bool liftStack(const PCodeInstruction& instr, LiftContext& ctx);
    bool liftMemory(const PCodeInstruction& instr, LiftContext& ctx);
    bool liftBranch(const PCodeInstruction& instr, LiftContext& ctx);
    bool liftCall(const PCodeInstruction& instr, LiftContext& ctx);
    bool liftReturn(const PCodeInstruction& instr, LiftContext& ctx);
    
    // Helper: Convert PCodeType to IRType
    IRType pcodeTypeToIRType(PCodeType type);
    
    // Helper: Get or create basic block for address
    IRBasicBlock* getOrCreateBlockForAddress(uint32_t address, LiftContext& ctx);
    
    // Helper: Resolve pending branch targets after all blocks are created
    void resolveBranches(LiftContext& ctx);
    
    void setError(const std::string& error) {
        lastError_ = error;
    }
    
    std::string lastError_;
};

} // namespace VBDecompiler
