// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/decompiler/Decompiler.h"
#include <iostream>

using namespace VBDecompiler;

int main() {
    std::cout << "VBDecompiler - Simple Decompiler Test\n";
    std::cout << "======================================\n\n";
    
    try {
        std::cout << "Creating IR function...\n";
        
        // Create function
        IRFunction func("Add", IRTypes::Integer);
        func.setAddress(0x00401000);
        
        std::cout << "Adding parameters...\n";
        
        // Add parameters
        IRVariable paramA(0, "a", IRTypes::Integer);
        IRVariable paramB(1, "b", IRTypes::Integer);
        func.addParameter(std::move(paramA));
        func.addParameter(std::move(paramB));
        
        std::cout << "Creating local variable...\n";
        
        // Create local variable
        IRVariable& result = func.createLocalVariable("result", IRTypes::Integer);
        
        std::cout << "Creating basic block...\n";
        
        // Create entry basic block
        IRBasicBlock& entryBlock = func.createBasicBlock();
        func.setEntryBlock(entryBlock.getId());
        
        std::cout << "Building expression: a + b...\n";
        
        // Build expression: a + b
        auto exprA = IRExpression::makeVariable(func.getParameters()[0]);
        auto exprB = IRExpression::makeVariable(func.getParameters()[1]);
        auto exprAdd = IRExpression::makeBinary(
            IRExpressionKind::ADD,
            std::move(exprA),
            std::move(exprB),
            IRTypes::Integer
        );
        
        std::cout << "Creating assignment statement...\n";
        
        // Statement: result = a + b
        auto assignStmt = IRStatement::makeAssign(result, std::move(exprAdd));
        entryBlock.addStatement(std::move(assignStmt));
        
        std::cout << "Creating return statement...\n";
        
        // Statement: Return result
        auto returnExpr = IRExpression::makeVariable(result);
        auto returnStmt = IRStatement::makeReturn(std::move(returnExpr));
        entryBlock.addStatement(std::move(returnStmt));
        
        std::cout << "IR function created successfully!\n\n";
        
        std::cout << "Creating decompiler...\n";
        Decompiler decompiler;
        
        std::cout << "Decompiling (without structuring)...\n";
        std::string vbCode = decompiler.decompile(func, false);
        
        std::cout << "\n=== Generated VB6 Code ===\n";
        std::cout << vbCode;
        std::cout << "==========================\n\n";
        
        std::cout << "✓ Test passed!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception\n";
        return 1;
    }
}
