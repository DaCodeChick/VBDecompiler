// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#include "ControlFlowStructurer.h"
#include <algorithm>
#include <queue>
#include <stack>

namespace VBDecompiler {

std::unique_ptr<StructuredNode> ControlFlowStructurer::structureFunction(const IRFunction& function) {
    // Get entry block
    const auto* entryBlock = function.getEntryBlock();
    if (!entryBlock) {
        return nullptr;
    }
    
    // Create root sequence node
    auto root = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
    
    // Get all blocks in execution order
    std::vector<const IRBasicBlock*> blocks;
    std::unordered_set<const IRBasicBlock*> visited;
    std::queue<const IRBasicBlock*> queue;
    
    queue.push(entryBlock);
    visited.insert(entryBlock);
    
    while (!queue.empty()) {
        const auto* block = queue.front();
        queue.pop();
        blocks.push_back(block);
        
        // Add successors
        for (uint32_t succId : block->getSuccessors()) {
            const auto* succBlock = getBlockById(succId, function);
            if (succBlock && visited.find(succBlock) == visited.end()) {
                visited.insert(succBlock);
                queue.push(succBlock);
            }
        }
    }
    
    // Analyze control flow patterns
    auto structured = analyzeRegion(blocks, function);
    return structured ? std::move(structured) : std::move(root);
}

bool ControlFlowStructurer::isReducible(const IRFunction& function) const {
    // A CFG is reducible if it has no irreducible loops
    // For simplicity, we'll consider all CFGs reducible for now
    // A full implementation would check for improper regions
    return true;
}

std::unique_ptr<StructuredNode> ControlFlowStructurer::analyzeRegion(
    const std::vector<const IRBasicBlock*>& blocks,
    const IRFunction& function
) {
    if (blocks.empty()) {
        return nullptr;
    }
    
    auto root = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
    
    // Process blocks in order
    std::unordered_set<const IRBasicBlock*> processed;
    
    for (const auto* block : blocks) {
        if (processed.find(block) != processed.end()) {
            continue;
        }
        
        // Try to match control flow patterns
        const IRBasicBlock* thenBlock = nullptr;
        const IRBasicBlock* elseBlock = nullptr;
        const IRBasicBlock* mergeBlock = nullptr;
        const IRBasicBlock* bodyBlock = nullptr;
        const IRBasicBlock* exitBlock = nullptr;
        
        // Try If-Then-Else
        if (matchIfThenElse(block, function, thenBlock, elseBlock, mergeBlock)) {
            auto ifNode = std::make_unique<StructuredNode>(StructuredNodeKind::IF_THEN_ELSE);
            
            // Get condition from branch statement
            if (!block->getStatements().empty()) {
                const auto* lastStmt = block->getStatements().back().get();
                if (lastStmt->getKind() == IRStatementKind::BRANCH) {
                    ifNode->setCondition(lastStmt->getCondition());
                }
            }
            
            ifNode->addBlock(block);
            if (thenBlock) {
                ifNode->addBlock(thenBlock);
                processed.insert(thenBlock);
            }
            if (elseBlock) {
                ifNode->addBlock(elseBlock);
                processed.insert(elseBlock);
            }
            
            root->addChild(std::move(ifNode));
            processed.insert(block);
            continue;
        }
        
        // Try If-Then (no else)
        if (matchIfThen(block, function, thenBlock, mergeBlock)) {
            auto ifNode = std::make_unique<StructuredNode>(StructuredNodeKind::IF_THEN);
            
            // Get condition from branch statement
            if (!block->getStatements().empty()) {
                const auto* lastStmt = block->getStatements().back().get();
                if (lastStmt->getKind() == IRStatementKind::BRANCH) {
                    ifNode->setCondition(lastStmt->getCondition());
                }
            }
            
            ifNode->addBlock(block);
            if (thenBlock) {
                ifNode->addBlock(thenBlock);
                processed.insert(thenBlock);
            }
            
            root->addChild(std::move(ifNode));
            processed.insert(block);
            continue;
        }
        
        // Try While loop
        if (matchWhileLoop(block, function, bodyBlock, exitBlock)) {
            auto whileNode = std::make_unique<StructuredNode>(StructuredNodeKind::WHILE);
            
            // Get condition from branch statement
            if (!block->getStatements().empty()) {
                const auto* lastStmt = block->getStatements().back().get();
                if (lastStmt->getKind() == IRStatementKind::BRANCH) {
                    whileNode->setCondition(lastStmt->getCondition());
                }
            }
            
            whileNode->addBlock(block);
            if (bodyBlock) {
                whileNode->addBlock(bodyBlock);
                processed.insert(bodyBlock);
            }
            
            root->addChild(std::move(whileNode));
            processed.insert(block);
            continue;
        }
        
        // Default: sequential block
        auto seqNode = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
        seqNode->addBlock(block);
        root->addChild(std::move(seqNode));
        processed.insert(block);
    }
    
    return root;
}

bool ControlFlowStructurer::matchIfThenElse(
    const IRBasicBlock* block,
    const IRFunction& function,
    const IRBasicBlock*& thenBlock,
    const IRBasicBlock*& elseBlock,
    const IRBasicBlock*& mergeBlock
) {
    // Pattern: block has 2 successors (then and else branches)
    // Both branches converge to a merge block
    
    const auto& successors = block->getSuccessors();
    if (successors.size() != 2) {
        return false;
    }
    
    // Check if last statement is a branch
    if (block->getStatements().empty()) {
        return false;
    }
    
    const auto* lastStmt = block->getStatements().back().get();
    if (lastStmt->getKind() != IRStatementKind::BRANCH) {
        return false;
    }
    
    // Get then and else blocks
    auto it = successors.begin();
    const auto* succ1 = getBlockById(*it, function);
    ++it;
    const auto* succ2 = getBlockById(*it, function);
    
    if (!succ1 || !succ2) {
        return false;
    }
    
    // Simple heuristic: first successor is then, second is else
    thenBlock = succ1;
    elseBlock = succ2;
    
    // Try to find merge block (common successor)
    const auto& thenSuccs = thenBlock->getSuccessors();
    const auto& elseSuccs = elseBlock->getSuccessors();
    
    for (uint32_t thenSuccId : thenSuccs) {
        for (uint32_t elseSuccId : elseSuccs) {
            if (thenSuccId == elseSuccId) {
                mergeBlock = getBlockById(thenSuccId, function);
                return true;
            }
        }
    }
    
    // No merge block found - still valid if one branch is empty
    return true;
}

bool ControlFlowStructurer::matchIfThen(
    const IRBasicBlock* block,
    const IRFunction& function,
    const IRBasicBlock*& thenBlock,
    const IRBasicBlock*& mergeBlock
) {
    // Pattern: block has 2 successors, one is the then branch, other is merge (fall-through)
    
    const auto& successors = block->getSuccessors();
    if (successors.size() != 2) {
        return false;
    }
    
    // Check if last statement is a branch
    if (block->getStatements().empty()) {
        return false;
    }
    
    const auto* lastStmt = block->getStatements().back().get();
    if (lastStmt->getKind() != IRStatementKind::BRANCH) {
        return false;
    }
    
    auto it = successors.begin();
    const auto* succ1 = getBlockById(*it, function);
    ++it;
    const auto* succ2 = getBlockById(*it, function);
    
    if (!succ1 || !succ2) {
        return false;
    }
    
    // Heuristic: branch target is then block, fall-through is merge
    thenBlock = succ1;
    mergeBlock = succ2;
    
    return true;
}

bool ControlFlowStructurer::matchWhileLoop(
    const IRBasicBlock* block,
    const IRFunction& function,
    const IRBasicBlock*& bodyBlock,
    const IRBasicBlock*& exitBlock
) {
    // Pattern: block has 2 successors, one loops back (body), one exits
    
    const auto& successors = block->getSuccessors();
    if (successors.size() != 2) {
        return false;
    }
    
    // Check if last statement is a branch
    if (block->getStatements().empty()) {
        return false;
    }
    
    const auto* lastStmt = block->getStatements().back().get();
    if (lastStmt->getKind() != IRStatementKind::BRANCH) {
        return false;
    }
    
    auto it = successors.begin();
    const auto* succ1 = getBlockById(*it, function);
    ++it;
    const auto* succ2 = getBlockById(*it, function);
    
    if (!succ1 || !succ2) {
        return false;
    }
    
    // Check if one successor loops back to this block (back edge)
    bool succ1IsBackEdge = isBackEdge(succ1, block, function);
    bool succ2IsBackEdge = isBackEdge(succ2, block, function);
    
    if (succ1IsBackEdge) {
        bodyBlock = succ1;
        exitBlock = succ2;
        return true;
    }
    
    if (succ2IsBackEdge) {
        bodyBlock = succ2;
        exitBlock = succ1;
        return true;
    }
    
    return false;
}

std::vector<const IRBasicBlock*> ControlFlowStructurer::getBlocksInPostOrder(const IRFunction& function) const {
    std::vector<const IRBasicBlock*> result;
    std::unordered_set<const IRBasicBlock*> visited;
    std::stack<const IRBasicBlock*> stack;
    
    const auto* entryBlock = function.getEntryBlock();
    if (!entryBlock) {
        return result;
    }
    
    stack.push(entryBlock);
    
    while (!stack.empty()) {
        const auto* block = stack.top();
        
        if (visited.find(block) != visited.end()) {
            stack.pop();
            result.push_back(block);
            continue;
        }
        
        visited.insert(block);
        
        // Push successors
        for (uint32_t succId : block->getSuccessors()) {
            const auto* succBlock = getBlockById(succId, function);
            if (succBlock && visited.find(succBlock) == visited.end()) {
                stack.push(succBlock);
            }
        }
    }
    
    return result;
}

std::unordered_set<const IRBasicBlock*> ControlFlowStructurer::getDominators(
    const IRBasicBlock* block,
    const IRFunction& function
) const {
    // Simplified dominator computation
    // A full implementation would use iterative dataflow analysis
    std::unordered_set<const IRBasicBlock*> dominators;
    dominators.insert(block);
    return dominators;
}

bool ControlFlowStructurer::isBackEdge(
    const IRBasicBlock* from,
    const IRBasicBlock* to,
    const IRFunction& function
) const {
    // A back edge is an edge from a block to one of its predecessors
    // Simplified: check if 'to' has a path back to 'from'
    
    const auto& fromSuccs = from->getSuccessors();
    for (uint32_t succId : fromSuccs) {
        if (succId == to->getId()) {
            // Direct edge from 'from' to 'to'
            // Check if 'to' can reach 'from' (cycle)
            const auto& toPreds = to->getPredecessors();
            if (toPreds.find(from->getId()) != toPreds.end()) {
                return true;
            }
        }
    }
    
    return false;
}

const IRBasicBlock* ControlFlowStructurer::getBlockById(uint32_t id, const IRFunction& function) const {
    return function.getBasicBlock(id);
}

} // namespace VBDecompiler
