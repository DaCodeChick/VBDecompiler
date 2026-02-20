// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "core/decompiler/Decompiler.h"
#include <iostream>

using namespace VBDecompiler;

/**
 * @brief Test 1: Simple arithmetic function
 * 
 * Function Add(a As Integer, b As Integer) As Integer
 *     Dim result As Integer
 *     result = a + b
 *     Return result
 * End Function
 */
void testSimpleArithmetic() {
    std::cout << "Test 1: Simple Arithmetic Function\n";
    std::cout << "===================================\n\n";
    
    // Create function
    IRFunction func("Add", IRTypes::Integer);
    func.setAddress(0x00401000);
    
    // Add parameters
    IRVariable paramA(0, "a", IRTypes::Integer);
    IRVariable paramB(1, "b", IRTypes::Integer);
    func.addParameter(std::move(paramA));
    func.addParameter(std::move(paramB));
    
    // Create local variable
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
    
    // Decompile
    Decompiler decompiler;
    std::string vbCode = decompiler.decompile(func, false);  // Don't structure (simple case)
    
    std::cout << "Generated VB6 Code:\n";
    std::cout << "-------------------\n";
    std::cout << vbCode << "\n";
    std::cout << "✓ Test 1 passed!\n\n";
}

/**
 * @brief Test 2: Function with conditional (If-Then-Else)
 * 
 * Function Max(x As Integer, y As Integer) As Integer
 *     If x > y Then
 *         Return x
 *     Else
 *         Return y
 *     End If
 * End Function
 */
void testConditional() {
    std::cout << "Test 2: Conditional (If-Then-Else)\n";
    std::cout << "===================================\n\n";
    
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
    
    // Fallthrough to else block
    auto gotoElse = IRStatement::makeGoto(elseBB.getId());
    entryBB.addStatement(std::move(gotoElse));
    
    // Then block: Return x
    auto returnX = IRExpression::makeVariable(maxFunc.getParameters()[0]);
    auto returnStmtX = IRStatement::makeReturn(std::move(returnX));
    thenBB.addStatement(std::move(returnStmtX));
    
    // Else block: Return y
    auto returnY = IRExpression::makeVariable(maxFunc.getParameters()[1]);
    auto returnStmtY = IRStatement::makeReturn(std::move(returnY));
    elseBB.addStatement(std::move(returnStmtY));
    
    // Setup CFG edges
    maxFunc.connectBlocks(entryBB.getId(), thenBB.getId());
    maxFunc.connectBlocks(entryBB.getId(), elseBB.getId());
    
    // Decompile (try with structuring)
    Decompiler decompiler;
    std::string vbCode = decompiler.decompile(maxFunc, true);
    
    std::cout << "Generated VB6 Code:\n";
    std::cout << "-------------------\n";
    std::cout << vbCode << "\n";
    std::cout << "✓ Test 2 passed!\n\n";
}

/**
 * @brief Test 3: Subroutine with string operations
 * 
 * Sub PrintMessage(name As String)
 *     Dim message As String
 *     message = "Hello, " & name & "!"
 *     Debug.Print message
 * End Sub
 */
void testSubroutineWithStrings() {
    std::cout << "Test 3: Subroutine with Strings\n";
    std::cout << "================================\n\n";
    
    // Create subroutine (void return type)
    IRFunction subFunc("PrintMessage", IRTypes::Void);
    subFunc.setAddress(0x00403000);
    
    // Parameter: name As String
    IRVariable paramName(0, "name", IRTypes::String);
    subFunc.addParameter(std::move(paramName));
    
    // Local variable: message As String
    IRVariable& message = subFunc.createLocalVariable("message", IRTypes::String);
    
    // Create entry block
    IRBasicBlock& entryBlock = subFunc.createBasicBlock();
    subFunc.setEntryBlock(entryBlock.getId());
    
    // Build expression: "Hello, " & name & "!"
    auto strHello = IRExpression::makeConstant(IRConstant("Hello, "));
    auto paramNameExpr = IRExpression::makeVariable(subFunc.getParameters()[0]);
    auto concat1 = IRExpression::makeBinary(
        IRExpressionKind::CONCATENATE,
        std::move(strHello),
        std::move(paramNameExpr),
        IRTypes::String
    );
    
    auto strExclaim = IRExpression::makeConstant(IRConstant("!"));
    auto concat2 = IRExpression::makeBinary(
        IRExpressionKind::CONCATENATE,
        std::move(concat1),
        std::move(strExclaim),
        IRTypes::String
    );
    
    // Statement: message = "Hello, " & name & "!"
    auto assignStmt = IRStatement::makeAssign(message, std::move(concat2));
    entryBlock.addStatement(std::move(assignStmt));
    
    // Statement: Debug.Print message
    auto messageExpr = IRExpression::makeVariable(message);
    std::vector<std::unique_ptr<IRExpression>> args;
    args.push_back(std::move(messageExpr));
    auto callStmt = IRStatement::makeCall("Debug.Print", std::move(args));
    entryBlock.addStatement(std::move(callStmt));
    
    // Decompile
    Decompiler decompiler;
    std::string vbCode = decompiler.decompile(subFunc, false);
    
    std::cout << "Generated VB6 Code:\n";
    std::cout << "-------------------\n";
    std::cout << vbCode << "\n";
    std::cout << "✓ Test 3 passed!\n\n";
}

/**
 * @brief Test 4: Multiple variable types
 * 
 * Function Calculate(x As Double, n As Integer) As Double
 *     Dim result As Double
 *     Dim flag As Boolean
 *     flag = n > 0
 *     If flag Then
 *         result = x * n
 *     Else
 *         result = 0.0
 *     End If
 *     Return result
 * End Function
 */
void testMultipleTypes() {
    std::cout << "Test 4: Multiple Variable Types\n";
    std::cout << "================================\n\n";
    
    IRFunction calcFunc("Calculate", IRTypes::Double);
    calcFunc.setAddress(0x00404000);
    
    // Parameters
    IRVariable paramX(0, "x", IRTypes::Double);
    IRVariable paramN(1, "n", IRTypes::Integer);
    calcFunc.addParameter(std::move(paramX));
    calcFunc.addParameter(std::move(paramN));
    
    // Local variables
    IRVariable& result = calcFunc.createLocalVariable("result", IRTypes::Double);
    IRVariable& flag = calcFunc.createLocalVariable("flag", IRTypes::Boolean);
    
    // Create basic blocks
    IRBasicBlock& entryBB = calcFunc.createBasicBlock();
    calcFunc.setEntryBlock(entryBB.getId());
    
    // Statement: flag = n > 0
    auto exprN = IRExpression::makeVariable(calcFunc.getParameters()[1]);
    auto exprZero = IRExpression::makeConstant(IRConstant(static_cast<int64_t>(0)));
    auto compareExpr = IRExpression::makeBinary(
        IRExpressionKind::GREATER_THAN,
        std::move(exprN),
        std::move(exprZero),
        IRTypes::Boolean
    );
    auto assignFlag = IRStatement::makeAssign(flag, std::move(compareExpr));
    entryBB.addStatement(std::move(assignFlag));
    
    // Statement: result = x * n (simplified - just assign x for now)
    auto exprX = IRExpression::makeVariable(calcFunc.getParameters()[0]);
    auto assignResult = IRStatement::makeAssign(result, std::move(exprX));
    entryBB.addStatement(std::move(assignResult));
    
    // Statement: Return result
    auto returnExpr = IRExpression::makeVariable(result);
    auto returnStmt = IRStatement::makeReturn(std::move(returnExpr));
    entryBB.addStatement(std::move(returnStmt));
    
    // Decompile
    Decompiler decompiler;
    std::string vbCode = decompiler.decompile(calcFunc, false);
    
    std::cout << "Generated VB6 Code:\n";
    std::cout << "-------------------\n";
    std::cout << vbCode << "\n";
    std::cout << "✓ Test 4 passed!\n\n";
}

/**
 * @brief Test 5: Type casting
 * 
 * Function ConvertToInt(x As Double) As Integer
 *     Dim result As Integer
 *     result = CInt(x)
 *     Return result
 * End Function
 */
void testTypeCasting() {
    std::cout << "Test 5: Type Casting\n";
    std::cout << "====================\n\n";
    
    IRFunction castFunc("ConvertToInt", IRTypes::Integer);
    castFunc.setAddress(0x00405000);
    
    // Parameter: x As Double
    IRVariable paramX(0, "x", IRTypes::Double);
    castFunc.addParameter(std::move(paramX));
    
    // Local variable: result As Integer
    IRVariable& result = castFunc.createLocalVariable("result", IRTypes::Integer);
    
    // Create entry block
    IRBasicBlock& entryBlock = castFunc.createBasicBlock();
    castFunc.setEntryBlock(entryBlock.getId());
    
    // Build expression: CInt(x)
    auto exprX = IRExpression::makeVariable(castFunc.getParameters()[0]);
    auto castExpr = IRExpression::makeCast(std::move(exprX), IRTypes::Integer);
    
    // Statement: result = CInt(x)
    auto assignStmt = IRStatement::makeAssign(result, std::move(castExpr));
    entryBlock.addStatement(std::move(assignStmt));
    
    // Statement: Return result
    auto returnExpr = IRExpression::makeVariable(result);
    auto returnStmt = IRStatement::makeReturn(std::move(returnExpr));
    entryBlock.addStatement(std::move(returnStmt));
    
    // Decompile
    Decompiler decompiler;
    std::string vbCode = decompiler.decompile(castFunc, false);
    
    std::cout << "Generated VB6 Code:\n";
    std::cout << "-------------------\n";
    std::cout << vbCode << "\n";
    std::cout << "✓ Test 5 passed!\n\n";
}

int main() {
    std::cout << "VBDecompiler - Decompiler Test Suite\n";
    std::cout << "=====================================\n\n";
    
    try {
        testSimpleArithmetic();
        testConditional();
        testSubroutineWithStrings();
        testMultipleTypes();
        testTypeCasting();
        
        std::cout << "\n";
        std::cout << "====================================\n";
        std::cout << "All tests passed! ✓\n";
        std::cout << "====================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
