#include "../src/core/ir/PCodeLifter.h"
#include "../src/core/disasm/pcode/PCodeDisassembler.h"
#include "../src/core/disasm/pcode/PCodeOpcode.h"
#include <iostream>
#include <vector>
#include <iomanip>

using namespace VBDecompiler;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " P-Code Lifter Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Test 1: Simple arithmetic expression
    std::cout << "Test 1: Simple Arithmetic (a + b)" << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    
    // Manually create P-Code instructions for: a + b
    // LitI4 10        ; Push 10
    // LitI4 20        ; Push 20
    // AddI4           ; Add them
    // ExitProc        ; Return
    
    std::vector<PCodeInstruction> instructions;
    
    // Instruction 1: LitI4 10
    PCodeInstruction instr1;
    instr1.setAddress(0x1000);
    instr1.setLength(5);
    instr1.setOpcode(0x10);  // Example opcode
    instr1.setMnemonic("LitI4");
    instr1.setCategory(PCodeOpcodeCategory::STACK);
    instr1.setStackDelta(1);  // Push
    PCodeOperand operand1(PCodeOperandType::INT32, static_cast<int32_t>(10), PCodeType::LONG);
    instr1.addOperand(operand1);
    instructions.push_back(instr1);
    
    // Instruction 2: LitI4 20
    PCodeInstruction instr2;
    instr2.setAddress(0x1005);
    instr2.setLength(5);
    instr2.setOpcode(0x10);
    instr2.setMnemonic("LitI4");
    instr2.setCategory(PCodeOpcodeCategory::STACK);
    instr2.setStackDelta(1);  // Push
    PCodeOperand operand2(PCodeOperandType::INT32, static_cast<int32_t>(20), PCodeType::LONG);
    instr2.addOperand(operand2);
    instructions.push_back(instr2);
    
    // Instruction 3: AddI4
    PCodeInstruction instr3;
    instr3.setAddress(0x100A);
    instr3.setLength(1);
    instr3.setOpcode(0x20);
    instr3.setMnemonic("AddI4");
    instr3.setCategory(PCodeOpcodeCategory::ARITHMETIC);
    instr3.setStackDelta(-1);  // Pop 2, push 1 = net -1
    instructions.push_back(instr3);
    
    // Instruction 4: ExitProc
    PCodeInstruction instr4;
    instr4.setAddress(0x100B);
    instr4.setLength(1);
    instr4.setOpcode(0xF0);
    instr4.setMnemonic("ExitProc");
    instr4.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    instr4.setIsReturn(true);
    instructions.push_back(instr4);
    
    // Lift to IR
    PCodeLifter lifter;
    auto irFunc = lifter.lift(instructions, "TestAdd", 0x1000);
    
    if (!irFunc) {
        std::cerr << "Error: Failed to lift P-Code to IR" << std::endl;
        std::cerr << "Reason: " << lifter.getLastError() << std::endl;
        return 1;
    }
    
    // Print IR
    std::cout << irFunc->toString() << std::endl;
    
    std::cout << "\n✓ Test 1 passed!\n" << std::endl;
    
    // Test 2: Conditional branch
    std::cout << "Test 2: Conditional Branch (If x > y)" << std::endl;
    std::cout << "---------------------------------------" << std::endl;
    
    std::vector<PCodeInstruction> branchInstructions;
    
    // LdLoc 0         ; Load local variable 0 (x)
    PCodeInstruction load1;
    load1.setAddress(0x2000);
    load1.setLength(2);
    load1.setMnemonic("LdLoc");
    load1.setCategory(PCodeOpcodeCategory::VARIABLE);
    load1.setStackDelta(1);
    PCodeOperand loadOp1(PCodeOperandType::INT16, static_cast<int16_t>(0), PCodeType::INTEGER);
    load1.addOperand(loadOp1);
    branchInstructions.push_back(load1);
    
    // LdLoc 1         ; Load local variable 1 (y)
    PCodeInstruction load2;
    load2.setAddress(0x2002);
    load2.setLength(2);
    load2.setMnemonic("LdLoc");
    load2.setCategory(PCodeOpcodeCategory::VARIABLE);
    load2.setStackDelta(1);
    PCodeOperand loadOp2(PCodeOperandType::INT16, static_cast<int16_t>(1), PCodeType::INTEGER);
    load2.addOperand(loadOp2);
    branchInstructions.push_back(load2);
    
    // GtI4            ; Greater than comparison
    PCodeInstruction cmp;
    cmp.setAddress(0x2004);
    cmp.setLength(1);
    cmp.setMnemonic("GtI4");
    cmp.setCategory(PCodeOpcodeCategory::COMPARISON);
    cmp.setStackDelta(-1);
    branchInstructions.push_back(cmp);
    
    // BranchTrue +5   ; Branch if true (to 0x200A)
    PCodeInstruction branch;
    branch.setAddress(0x2005);
    branch.setLength(2);
    branch.setMnemonic("BranchTrue");
    branch.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    branch.setIsBranch(true);
    branch.setIsConditionalBranch(true);
    branch.setBranchOffset(5);
    branchInstructions.push_back(branch);
    
    // LitI4 0         ; Push 0 (else branch)
    PCodeInstruction lit0;
    lit0.setAddress(0x2007);
    lit0.setLength(5);
    lit0.setMnemonic("LitI4");
    lit0.setCategory(PCodeOpcodeCategory::STACK);
    lit0.setStackDelta(1);
    PCodeOperand lit0Op(PCodeOperandType::INT32, static_cast<int32_t>(0), PCodeType::LONG);
    lit0.addOperand(lit0Op);
    branchInstructions.push_back(lit0);
    
    // Branch +3       ; Skip to ExitProc
    PCodeInstruction skip;
    skip.setAddress(0x200C);
    skip.setLength(2);
    skip.setMnemonic("Branch");
    skip.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    skip.setIsBranch(true);
    skip.setIsConditionalBranch(false);
    skip.setBranchOffset(3);
    branchInstructions.push_back(skip);
    
    // LitI4 1         ; Push 1 (then branch - address 0x200A + 5 = 0x200F)
    PCodeInstruction lit1;
    lit1.setAddress(0x200E);  // This is the branch target
    lit1.setLength(5);
    lit1.setMnemonic("LitI4");
    lit1.setCategory(PCodeOpcodeCategory::STACK);
    lit1.setStackDelta(1);
    PCodeOperand lit1Op(PCodeOperandType::INT32, static_cast<int32_t>(1), PCodeType::LONG);
    lit1.addOperand(lit1Op);
    branchInstructions.push_back(lit1);
    
    // ExitProc
    PCodeInstruction exit;
    exit.setAddress(0x2013);
    exit.setLength(1);
    exit.setMnemonic("ExitProc");
    exit.setCategory(PCodeOpcodeCategory::CONTROL_FLOW);
    exit.setIsReturn(true);
    branchInstructions.push_back(exit);
    
    // Lift to IR
    auto irBranchFunc = lifter.lift(branchInstructions, "TestBranch", 0x2000);
    
    if (!irBranchFunc) {
        std::cerr << "Error: Failed to lift P-Code to IR" << std::endl;
        std::cerr << "Reason: " << lifter.getLastError() << std::endl;
        return 1;
    }
    
    // Print IR
    std::cout << irBranchFunc->toString() << std::endl;
    
    std::cout << "\n✓ Test 2 passed!\n" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
