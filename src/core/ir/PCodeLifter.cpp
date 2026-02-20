// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PCodeLifter.h"
#include "../disasm/pcode/PCodeOpcode.h"
#include <sstream>

namespace VBDecompiler {

std::unique_ptr<IRFunction> PCodeLifter::lift(
    const std::vector<PCodeInstruction>& instructions,
    const std::string& functionName,
    uint32_t startAddress)
{
    if (instructions.empty()) {
        setError("No instructions to lift");
        return nullptr;
    }
    
    // Create lifting context
    LiftContext ctx;
    ctx.function = std::make_unique<IRFunction>(functionName, IRTypes::Variant);
    ctx.function->setAddress(startAddress);
    
    // Create entry block
    ctx.currentBlock = &ctx.function->createBasicBlock();
    ctx.function->setEntryBlock(ctx.currentBlock->getId());
    
    // First pass: identify basic block boundaries (branch targets)
    for (const auto& instr : instructions) {
        if (instr.isBranch() && instr.getBranchOffset() != 0) {
            uint32_t targetAddr = instr.getAddress() + instr.getLength() + instr.getBranchOffset();
            getOrCreateBlockForAddress(targetAddr, ctx);
        }
    }
    
    // Second pass: lift instructions
    for (const auto& instr : instructions) {
        // Check if this address starts a new block
        auto it = ctx.addressToBlock.find(instr.getAddress());
        if (it != ctx.addressToBlock.end() && it->second != ctx.currentBlock) {
            // Connect current block to new block
            if (ctx.currentBlock && !ctx.currentBlock->isEmpty()) {
                ctx.function->connectBlocks(ctx.currentBlock->getId(), it->second->getId());
            }
            ctx.currentBlock = it->second;
        }
        
        // Lift the instruction
        if (!liftInstruction(instr, ctx)) {
            setError("Failed to lift instruction: " + instr.getMnemonic());
            return nullptr;
        }
        
        // Stop at return
        if (instr.isReturn()) {
            break;
        }
    }
    
    // Third pass: resolve all branch targets
    resolveBranches(ctx);
    
    return std::move(ctx.function);
}

bool PCodeLifter::liftInstruction(const PCodeInstruction& instr, LiftContext& ctx) {
    auto category = instr.getCategory();
    
    // Route to specialized lifters based on category
    switch (category) {
        case PCodeOpcodeCategory::ARITHMETIC:
            return liftArithmetic(instr, ctx);
        case PCodeOpcodeCategory::COMPARISON:
            return liftComparison(instr, ctx);
        case PCodeOpcodeCategory::LOGICAL:
            return liftLogical(instr, ctx);
        case PCodeOpcodeCategory::STACK:
        case PCodeOpcodeCategory::VARIABLE:
            return liftStack(instr, ctx);
        case PCodeOpcodeCategory::MEMORY:
        case PCodeOpcodeCategory::ARRAY:
            return liftMemory(instr, ctx);
        case PCodeOpcodeCategory::CONTROL_FLOW:
            // Handle control flow (branch, return, exit)
            if (instr.isBranch()) {
                return liftBranch(instr, ctx);
            } else if (instr.isReturn() || 
                       instr.getMnemonic().find("Exit") != std::string::npos ||
                       instr.getMnemonic().find("Return") != std::string::npos) {
                return liftReturn(instr, ctx);
            }
            return true;  // Ignore other control flow for now
        case PCodeOpcodeCategory::CALL:
            return liftCall(instr, ctx);
        default:
            return true;  // Ignore unknown categories
    }
}

bool PCodeLifter::liftArithmetic(const PCodeInstruction& instr, LiftContext& ctx) {
    const auto& mnemonic = instr.getMnemonic();
    
    // Arithmetic operations pop 2 operands, push result
    // Map P-Code arithmetic to IR binary operations
    IRExpressionKind op;
    if (mnemonic.find("Add") != std::string::npos) {
        op = IRExpressionKind::ADD;
    } else if (mnemonic.find("Sub") != std::string::npos) {
        op = IRExpressionKind::SUBTRACT;
    } else if (mnemonic.find("Mul") != std::string::npos) {
        op = IRExpressionKind::MULTIPLY;
    } else if (mnemonic.find("Div") != std::string::npos) {
        op = IRExpressionKind::DIVIDE;
    } else if (mnemonic.find("Idiv") != std::string::npos) {
        op = IRExpressionKind::INT_DIVIDE;
    } else if (mnemonic.find("Mod") != std::string::npos) {
        op = IRExpressionKind::MODULO;
    } else if (mnemonic.find("Concat") != std::string::npos) {
        op = IRExpressionKind::CONCATENATE;
    } else {
        return true;  // Unknown arithmetic, skip
    }
    
    // Pop operands (right then left, since it's a stack)
    auto right = ctx.popStack();
    auto left = ctx.popStack();
    if (!right || !left) {
        return false;
    }
    
    // Create binary expression
    auto result = IRExpression::makeBinary(op, std::move(left), std::move(right), IRTypes::Variant);
    
    // Push result
    ctx.pushStack(std::move(result));
    
    return true;
}

bool PCodeLifter::liftComparison(const PCodeInstruction& instr, LiftContext& ctx) {
    const auto& mnemonic = instr.getMnemonic();
    
    // Comparison operations pop 2 operands, push boolean result
    IRExpressionKind op;
    if (mnemonic.find("Eq") != std::string::npos) {
        op = IRExpressionKind::EQUAL;
    } else if (mnemonic.find("Ne") != std::string::npos) {
        op = IRExpressionKind::NOT_EQUAL;
    } else if (mnemonic.find("Lt") != std::string::npos) {
        op = IRExpressionKind::LESS_THAN;
    } else if (mnemonic.find("Le") != std::string::npos) {
        op = IRExpressionKind::LESS_EQUAL;
    } else if (mnemonic.find("Gt") != std::string::npos) {
        op = IRExpressionKind::GREATER_THAN;
    } else if (mnemonic.find("Ge") != std::string::npos) {
        op = IRExpressionKind::GREATER_EQUAL;
    } else {
        return true;  // Unknown comparison, skip
    }
    
    // Pop operands
    auto right = ctx.popStack();
    auto left = ctx.popStack();
    if (!right || !left) {
        return false;
    }
    
    // Create comparison expression
    auto result = IRExpression::makeBinary(op, std::move(left), std::move(right), IRTypes::Boolean);
    
    // Push result
    ctx.pushStack(std::move(result));
    
    return true;
}

bool PCodeLifter::liftLogical(const PCodeInstruction& instr, LiftContext& ctx) {
    const auto& mnemonic = instr.getMnemonic();
    
    // Logical operations
    if (mnemonic.find("Not") != std::string::npos) {
        // Unary NOT
        auto operand = ctx.popStack();
        if (!operand) {
            return false;
        }
        auto result = IRExpression::makeUnary(IRExpressionKind::NOT, std::move(operand), IRTypes::Boolean);
        ctx.pushStack(std::move(result));
        return true;
    }
    
    IRExpressionKind op;
    if (mnemonic.find("And") != std::string::npos) {
        op = IRExpressionKind::AND;
    } else if (mnemonic.find("Or") != std::string::npos) {
        op = IRExpressionKind::OR;
    } else if (mnemonic.find("Xor") != std::string::npos) {
        op = IRExpressionKind::XOR;
    } else {
        return true;  // Unknown logical, skip
    }
    
    // Binary logical operations
    auto right = ctx.popStack();
    auto left = ctx.popStack();
    if (!right || !left) {
        return false;
    }
    
    auto result = IRExpression::makeBinary(op, std::move(left), std::move(right), IRTypes::Boolean);
    ctx.pushStack(std::move(result));
    
    return true;
}

bool PCodeLifter::liftStack(const PCodeInstruction& instr, LiftContext& ctx) {
    const auto& mnemonic = instr.getMnemonic();
    const auto& operands = instr.getOperands();
    
    // Handle literal pushes
    if (mnemonic.find("Lit") != std::string::npos) {
        if (operands.empty()) {
            return false;
        }
        
        const auto& operand = operands[0];
        std::unique_ptr<IRExpression> expr;
        
        // Create constant based on operand type
        switch (operand.type) {
            case PCodeOperandType::BYTE: {
                IRConstant constant(static_cast<int64_t>(operand.getByte()));
                expr = IRExpression::makeConstant(constant);
                break;
            }
            case PCodeOperandType::INT16: {
                IRConstant constant(static_cast<int64_t>(operand.getInt16()));
                expr = IRExpression::makeConstant(constant);
                break;
            }
            case PCodeOperandType::INT32: {
                IRConstant constant(static_cast<int64_t>(operand.getInt32()));
                expr = IRExpression::makeConstant(constant);
                break;
            }
            case PCodeOperandType::FLOAT: {
                IRConstant constant(static_cast<double>(operand.getFloat()));
                expr = IRExpression::makeConstant(constant);
                break;
            }
            case PCodeOperandType::STRING: {
                IRConstant constant(operand.getString());
                expr = IRExpression::makeConstant(constant);
                break;
            }
            default:
                return false;
        }
        
        ctx.pushStack(std::move(expr));
        return true;
    }
    
    // Handle local variable loads
    if (mnemonic.find("LdLoc") != std::string::npos || mnemonic.find("LoadLocal") != std::string::npos) {
        if (operands.empty()) {
            return false;
        }
        
        // Create variable reference
        int16_t localIndex = operands[0].getInt16();
        std::string varName = "local" + std::to_string(localIndex);
        IRType varType = pcodeTypeToIRType(operands[0].dataType);
        
        IRVariable varRef(localIndex, varName, varType);
        auto expr = IRExpression::makeVariable(varRef);
        ctx.pushStack(std::move(expr));
        return true;
    }
    
    // Handle local variable stores
    if (mnemonic.find("StLoc") != std::string::npos || mnemonic.find("StoreLocal") != std::string::npos) {
        if (operands.empty()) {
            return false;
        }
        
        auto value = ctx.popStack();
        if (!value) {
            return false;
        }
        
        // Create assignment statement
        int16_t localIndex = operands[0].getInt16();
        std::string varName = "local" + std::to_string(localIndex);
        IRType varType = pcodeTypeToIRType(operands[0].dataType);
        
        IRVariable var(localIndex, varName, varType);
        auto stmt = IRStatement::makeAssign(std::move(var), std::move(value));
        ctx.currentBlock->addStatement(std::move(stmt));
        return true;
    }
    
    return true;
}

bool PCodeLifter::liftMemory(const PCodeInstruction& instr, LiftContext& ctx) {
    // Memory operations - to be implemented when needed
    // For now, just skip them
    return true;
}

bool PCodeLifter::liftBranch(const PCodeInstruction& instr, LiftContext& ctx) {
    const auto& mnemonic = instr.getMnemonic();
    
    // Calculate branch target address
    uint32_t targetAddr = instr.getAddress() + instr.getLength() + instr.getBranchOffset();
    
    if (instr.isConditionalBranch()) {
        // Pop condition from stack
        auto condition = ctx.popStack();
        if (!condition) {
            return false;
        }
        
        // Get or create target block
        auto* targetBlock = getOrCreateBlockForAddress(targetAddr, ctx);
        
        // Create branch statement
        auto stmt = IRStatement::makeBranch(std::move(condition), targetBlock->getId());
        ctx.currentBlock->addStatement(std::move(stmt));
        
        // Connect to target
        ctx.function->connectBlocks(ctx.currentBlock->getId(), targetBlock->getId());
        
        // Create fall-through block
        auto& fallThroughBlock = ctx.function->createBasicBlock();
        ctx.function->connectBlocks(ctx.currentBlock->getId(), fallThroughBlock.getId());
        ctx.currentBlock = &fallThroughBlock;
    } else {
        // Unconditional branch (goto)
        auto* targetBlock = getOrCreateBlockForAddress(targetAddr, ctx);
        
        auto stmt = IRStatement::makeGoto(targetBlock->getId());
        ctx.currentBlock->addStatement(std::move(stmt));
        
        // Connect to target
        ctx.function->connectBlocks(ctx.currentBlock->getId(), targetBlock->getId());
        
        // Create new block for any following code
        ctx.currentBlock = &ctx.function->createBasicBlock();
    }
    
    return true;
}

bool PCodeLifter::liftCall(const PCodeInstruction& instr, LiftContext& ctx) {
    const auto& mnemonic = instr.getMnemonic();
    const auto& operands = instr.getOperands();
    
    // Extract function name/address
    std::string funcName = "func_unknown";
    if (!operands.empty()) {
        if (operands[0].type == PCodeOperandType::ADDRESS) {
            funcName = "func_" + std::to_string(operands[0].getInt32());
        } else if (operands[0].type == PCodeOperandType::STRING) {
            funcName = operands[0].getString();
        }
    }
    
    // For now, create a simple call with no arguments
    // TODO: Pop arguments from stack based on calling convention
    std::vector<std::unique_ptr<IRExpression>> args;
    
    // If this is a function call (not sub), create call expression and push result
    if (mnemonic.find("CallFunc") != std::string::npos || mnemonic.find("CallI4") != std::string::npos) {
        auto callExpr = IRExpression::makeCall(funcName, std::move(args), IRTypes::Variant);
        ctx.pushStack(std::move(callExpr));
    } else {
        // It's a subroutine call, create a call statement
        auto stmt = IRStatement::makeCall(funcName, std::move(args));
        ctx.currentBlock->addStatement(std::move(stmt));
    }
    
    return true;
}

bool PCodeLifter::liftReturn(const PCodeInstruction& instr, LiftContext& ctx) {
    const auto& mnemonic = instr.getMnemonic();
    
    // Check if this is a function return (with value) or sub return (no value)
    if (mnemonic.find("ExitProc") != std::string::npos) {
        // Sub return - no value
        auto stmt = IRStatement::makeReturn(nullptr);
        ctx.currentBlock->addStatement(std::move(stmt));
    } else {
        // Function return - pop return value
        auto retValue = ctx.popStack();
        auto stmt = IRStatement::makeReturn(std::move(retValue));
        ctx.currentBlock->addStatement(std::move(stmt));
    }
    
    return true;
}

IRType PCodeLifter::pcodeTypeToIRType(PCodeType type) {
    switch (type) {
        case PCodeType::BYTE:
            return IRTypes::Byte;
        case PCodeType::BOOLEAN:
            return IRTypes::Boolean;
        case PCodeType::INTEGER:
            return IRTypes::Integer;
        case PCodeType::LONG:
            return IRTypes::Long;
        case PCodeType::SINGLE:
            return IRTypes::Single;
        case PCodeType::STRING:
            return IRTypes::String;
        case PCodeType::OBJECT:
            return IRTypes::Object;
        case PCodeType::VARIANT:
        case PCodeType::UNKNOWN:
        default:
            return IRTypes::Variant;
    }
}

IRBasicBlock* PCodeLifter::getOrCreateBlockForAddress(uint32_t address, LiftContext& ctx) {
    auto it = ctx.addressToBlock.find(address);
    if (it != ctx.addressToBlock.end()) {
        return it->second;
    }
    
    // Create new block
    auto& block = ctx.function->createBasicBlock();
    ctx.addressToBlock[address] = &block;
    return &block;
}

void PCodeLifter::resolveBranches(LiftContext& ctx) {
    // Branches are resolved during lifting in this implementation
    // This method is kept for potential future use
}

} // namespace VBDecompiler
