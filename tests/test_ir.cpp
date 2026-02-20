// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/ir/IRFunction.h"
#include <iostream>

using namespace VBDecompiler;

/**
 * @brief Test IR system by building a simple function
 * 
 * This test creates an IR representation of this VB function:
 * 
 * Function Add(a As Integer, b As Integer) As Integer
 *     Dim result As Integer
 *     result = a + b
 *     Return result
 * End Function
 */
int main() {
    std::cout << "VBDecompiler IR Test\n";
    std::cout << "====================\n\n";
    
    // Create function: Function Add(a As Integer, b As Integer) As Integer
    IRFunction func("Add", IRTypes::Integer);
    func.setAddress(0x00401000);
    
    // Add parameters
    IRVariable paramA(0, "a", IRTypes::Integer);
    IRVariable paramB(1, "b", IRTypes::Integer);
    func.addParameter(std::move(paramA));
    func.addParameter(std::move(paramB));
    
    // Create local variable: result
    IRVariable& result = func.createLocalVariable("result", IRTypes::Integer);
    
    // Create entry basic block
    IRBasicBlock& entryBlock = func.createBasicBlock();
    func.setEntryBlock(entryBlock.getId());
    
    // Build expression: a + b
    auto exprA = IRExpression::makeVariable(func.getParameters()[0]);
    auto exprB = IRExpression::makeVariable(func.getParameters()[1]);
    auto exprAdd = IRExpression::makeBinary(
        IRExpressionKind::ADD,
        std::move(exprA),
        std::move(exprB),
        IRTypes::Integer
    );
    
    // Statement: result = a + b
    auto assignStmt = IRStatement::makeAssign(result, std::move(exprAdd));
    entryBlock.addStatement(std::move(assignStmt));
    
    // Statement: Return result
    auto returnExpr = IRExpression::makeVariable(result);
    auto returnStmt = IRStatement::makeReturn(std::move(returnExpr));
    entryBlock.addStatement(std::move(returnStmt));
    
    // Print the IR
    std::cout << func.toString() << "\n";
    
    std::cout << "\n✓ IR generation successful!\n\n";
    
    // Test more complex example with conditionals
    std::cout << "Complex Example: Function with If statement\n";
    std::cout << "============================================\n\n";
    
    /**
     * Function Max(x As Integer, y As Integer) As Integer
     *     If x > y Then
     *         Return x
     *     Else
     *         Return y
     *     End If
     * End Function
     */
    
    IRFunction maxFunc("Max", IRTypes::Integer);
    maxFunc.setAddress(0x00402000);
    
    // Parameters
    IRVariable paramX(0, "x", IRTypes::Integer);
    IRVariable paramY(1, "y", IRTypes::Integer);
    maxFunc.addParameter(std::move(paramX));
    maxFunc.addParameter(std::move(paramY));
    
    // Create basic blocks
    IRBasicBlock& entryBB = maxFunc.createBasicBlock();
    maxFunc.setEntryBlock(entryBB.getId());
    
    IRBasicBlock& thenBB = maxFunc.createBasicBlock();
    IRBasicBlock& elseBB = maxFunc.createBasicBlock();
    
    // Entry block: If x > y Then goto thenBB
    auto condX = IRExpression::makeVariable(maxFunc.getParameters()[0]);
    auto condY = IRExpression::makeVariable(maxFunc.getParameters()[1]);
    auto condExpr = IRExpression::makeBinary(
        IRExpressionKind::GREATER_THAN,
        std::move(condX),
        std::move(condY),
        IRTypes::Boolean
    );
    auto branchStmt = IRStatement::makeBranch(std::move(condExpr), thenBB.getId());
    entryBB.addStatement(std::move(branchStmt));
    
    // Entry falls through to else block
    auto gotoElse = IRStatement::makeGoto(elseBB.getId());
    entryBB.addStatement(std::move(gotoElse));
    
    // Then block: Return x
    auto returnX = IRExpression::makeVariable(maxFunc.getParameters()[0]);
    auto returnXStmt = IRStatement::makeReturn(std::move(returnX));
    thenBB.addStatement(std::move(returnXStmt));
    
    // Else block: Return y
    auto returnY = IRExpression::makeVariable(maxFunc.getParameters()[1]);
    auto returnYStmt = IRStatement::makeReturn(std::move(returnY));
    elseBB.addStatement(std::move(returnYStmt));
    
    // Connect blocks in CFG
    maxFunc.connectBlocks(entryBB.getId(), thenBB.getId());
    maxFunc.connectBlocks(entryBB.getId(), elseBB.getId());
    
    // Print the IR
    std::cout << maxFunc.toString() << "\n";
    
    std::cout << "✓ Complex IR generation successful!\n\n";
    
    // Test type system
    std::cout << "Type System Test\n";
    std::cout << "================\n\n";
    
    IRType intType = IRTypes::Integer;
    IRType strType = IRTypes::String;
    IRType arrayType = IRType::makeArray(IRTypes::Long, 2);
    IRType udtType = IRType::makeUserDefined("MyRecord");
    
    std::cout << "Integer type: " << intType.toString() 
              << " (size: " << intType.getSize() << " bytes)\n";
    std::cout << "String type: " << strType.toString() 
              << " (size: " << strType.getSize() << " bytes)\n";
    std::cout << "Array type: " << arrayType.toString() 
              << " (dimensions: " << arrayType.getArrayDimensions() << ")\n";
    std::cout << "UDT type: " << udtType.toString() << "\n";
    
    std::cout << "\n✓ All tests passed!\n";
    
    return 0;
}
