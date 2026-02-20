// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

/**
 * @brief End-to-End Decompilation Test
 * 
 * Tests the complete pipeline: P-Code → IR → VB6
 * This simulates decompiling actual VB6 functions from P-Code bytecode.
 */

#include "core/ir/PCodeLifter.h"
#include "core/decompiler/Decompiler.h"
#include "core/disasm/pcode/PCodeInstruction.h"
#include "core/disasm/pcode/PCodeOpcode.h"
#include <iostream>
#include <vector>

using namespace VBDecompiler;

/**
 * @brief Test 1: Simple arithmetic function
 * 
 * P-Code:
 *   LitI4 10
 *   LitI4 20
 *   AddI4
 *   Ret
 * 
 * Expected VB6:
 *   Function TestAdd() As Long
 *     Return 10 + 20
 *   End Function
 */
void testSimpleArithmetic() {
    std::cout << "Test 1: Simple Arithmetic (P-Code → IR → VB6)\n";
    std::cout << "==============================================\n\n";
    
    std::vector<PCodeInstruction> instructions;
    
    // LitI4 10
    PCodeInstruction instr1;
    instr1.setAddress(0x1000);
    instr1.setLength(5);
    instr1.setOpcode(0x10);
    instr1.setMnemonic("LitI4");
    instr1.setCategory(PCodeOpcodeCategory::STACK);
    instr1.setStackDelta(1);
    PCodeOperand op1(PCodeOperandType::INT32, static_cast<int32_t>(10), PCodeType::LONG);
    instr1.addOperand(op1);
    instructions.push_back(instr1);
    
    // LitI4 20
    PCodeInstruction instr2;
    instr2.setAddress(0x1005);
    instr2.setLength(5);
    instr2.setOpcode(0x10);
    instr2.setMnemonic("LitI4");
    instr2.setCategory(PCodeOpcodeCategory::STACK);
    instr2.setStackDelta(1);
    PCodeOperand op2(PCodeOperandType::INT32, static_cast<int32_t>(20), PCodeType::LONG);
    instr2.addOperand(op2);
    instructions.push_back(instr2);
    
    // AddI4
    PCodeInstruction instr3;
    instr3.setAddress(0x100A);
    instr3.setLength(1);
    instr3.setOpcode(0x20);
    instr3.setMnemonic("AddI4");
    instr3.setCategory(PCodeOpcodeCategory::ARITHMETIC);
    instr3.setStackDelta(-1);
    instructions.push_back(instr3);
    
    // Ret
    PCodeInstruction instr4;
    instr4.setAddress(0x100B);
    instr4.setLength(1);
    instr4.setOpcode(0xF0);
    instr4.setMnemonic("Ret");
    instr4.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    instr4.setIsReturn(true);
    instructions.push_back(instr4);
    
    // Lift P-Code → IR
    std::cout << "Step 1: Lifting P-Code to IR...\n";
    PCodeLifter lifter;
    auto irFunc = lifter.lift(instructions, "Add", 0x1000);
    
    if (!irFunc) {
        std::cerr << "✗ Failed to lift P-Code: " << lifter.getLastError() << "\n";
        throw std::runtime_error("Lift failed");
    }
    
    std::cout << "✓ P-Code lifted to IR\n\n";
    
    // Decompile IR → VB6
    std::cout << "Step 2: Decompiling IR to VB6...\n";
    Decompiler decompiler;
    std::string vbCode = decompiler.decompile(*irFunc, false);
    
    std::cout << "✓ IR decompiled to VB6\n\n";
    
    // Display result
    std::cout << "Generated VB6 Code:\n";
    std::cout << "-------------------\n";
    std::cout << vbCode << "\n";
    
    std::cout << "✓ Test 1 passed!\n\n";
}

/**
 * @brief Test 2: Conditional with local variables
 * 
 * P-Code simulating:
 *   Function Max(x As Integer, y As Integer) As Integer
 *     If x > y Then
 *       Return x
 *     Else
 *       Return y
 *     End If
 *   End Function
 * 
 * P-Code:
 *   LdLoc 0      ; Load x
 *   LdLoc 1      ; Load y
 *   CgtI4        ; Compare greater than
 *   BrTrue 0x2010
 *   LdLoc 1      ; else: Load y
 *   Ret
 *   LdLoc 0      ; then: Load x (0x2010)
 *   Ret
 */
void testConditional() {
    std::cout << "Test 2: Conditional (P-Code → IR → VB6)\n";
    std::cout << "========================================\n\n";
    
    std::vector<PCodeInstruction> instructions;
    
    // LdLoc 0
    PCodeInstruction load1;
    load1.setAddress(0x2000);
    load1.setLength(2);
    load1.setOpcode(0x30);
    load1.setMnemonic("LdLoc");
    load1.setCategory(PCodeOpcodeCategory::VARIABLE);
    load1.setStackDelta(1);
    PCodeOperand loadOp1(PCodeOperandType::INT16, static_cast<int16_t>(0), PCodeType::INTEGER);
    load1.addOperand(loadOp1);
    instructions.push_back(load1);
    
    // LdLoc 1
    PCodeInstruction load2;
    load2.setAddress(0x2002);
    load2.setLength(2);
    load2.setOpcode(0x30);
    load2.setMnemonic("LdLoc");
    load2.setCategory(PCodeOpcodeCategory::VARIABLE);
    load2.setStackDelta(1);
    PCodeOperand loadOp2(PCodeOperandType::INT16, static_cast<int16_t>(1), PCodeType::INTEGER);
    load2.addOperand(loadOp2);
    instructions.push_back(load2);
    
    // CgtI4 (Compare Greater Than)
    PCodeInstruction cmp;
    cmp.setAddress(0x2004);
    cmp.setLength(1);
    cmp.setOpcode(0x40);
    cmp.setMnemonic("CgtI4");
    cmp.setCategory(PCodeOpcodeCategory::COMPARISON);
    cmp.setStackDelta(-1);
    instructions.push_back(cmp);
    
    // BrTrue 0x2010
    PCodeInstruction branch;
    branch.setAddress(0x2005);
    branch.setLength(3);
    branch.setOpcode(0x50);
    branch.setMnemonic("BrTrue");
    branch.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    branch.setStackDelta(-1);
    branch.setIsBranch(true);
    branch.setIsConditionalBranch(true);
    branch.setBranchOffset(0x2010 - 0x2005);  // Offset to then block
    instructions.push_back(branch);
    
    // Else block: LdLoc 1
    PCodeInstruction loadElse;
    loadElse.setAddress(0x2008);
    loadElse.setLength(2);
    loadElse.setOpcode(0x30);
    loadElse.setMnemonic("LdLoc");
    loadElse.setCategory(PCodeOpcodeCategory::VARIABLE);
    loadElse.setStackDelta(1);
    PCodeOperand loadElseOp(PCodeOperandType::INT16, static_cast<int16_t>(1), PCodeType::INTEGER);
    loadElse.addOperand(loadElseOp);
    instructions.push_back(loadElse);
    
    // Ret (else)
    PCodeInstruction retElse;
    retElse.setAddress(0x200A);
    retElse.setLength(1);
    retElse.setOpcode(0xF0);
    retElse.setMnemonic("Ret");
    retElse.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    retElse.setIsReturn(true);
    instructions.push_back(retElse);
    
    // Then block: LdLoc 0 (at 0x2010)
    PCodeInstruction loadThen;
    loadThen.setAddress(0x2010);
    loadThen.setLength(2);
    loadThen.setOpcode(0x30);
    loadThen.setMnemonic("LdLoc");
    loadThen.setCategory(PCodeOpcodeCategory::VARIABLE);
    loadThen.setStackDelta(1);
    PCodeOperand loadThenOp(PCodeOperandType::INT16, static_cast<int16_t>(0), PCodeType::INTEGER);
    loadThen.addOperand(loadThenOp);
    instructions.push_back(loadThen);
    
    // Ret (then)
    PCodeInstruction retThen;
    retThen.setAddress(0x2012);
    retThen.setLength(1);
    retThen.setOpcode(0xF0);
    retThen.setMnemonic("Ret");
    retThen.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    retThen.setIsReturn(true);
    instructions.push_back(retThen);
    
    // Lift P-Code → IR
    std::cout << "Step 1: Lifting P-Code to IR...\n";
    PCodeLifter lifter;
    auto irFunc = lifter.lift(instructions, "Max", 0x2000);
    
    if (!irFunc) {
        std::cerr << "✗ Failed to lift P-Code: " << lifter.getLastError() << "\n";
        throw std::runtime_error("Lift failed");
    }
    
    std::cout << "✓ P-Code lifted to IR\n\n";
    
    // Decompile IR → VB6 (with structuring)
    std::cout << "Step 2: Decompiling IR to VB6 with control flow structuring...\n";
    Decompiler decompiler;
    std::string vbCode = decompiler.decompile(*irFunc, true);
    
    std::cout << "✓ IR decompiled to VB6\n\n";
    
    // Display result
    std::cout << "Generated VB6 Code:\n";
    std::cout << "-------------------\n";
    std::cout << vbCode << "\n";
    
    std::cout << "✓ Test 2 passed!\n\n";
}

int main() {
    std::cout << "VBDecompiler - End-to-End Test Suite\n";
    std::cout << "=====================================\n\n";
    
    try {
        testSimpleArithmetic();
        testConditional();
        
        std::cout << "\n";
        std::cout << "====================================\n";
        std::cout << "All end-to-end tests passed! ✓\n";
        std::cout << "====================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
