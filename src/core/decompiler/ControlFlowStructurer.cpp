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
        
        // Try While loop (check before If/Then/Else since loops have back edges)
        if (matchWhileLoop(block, function, bodyBlock, exitBlock)) {
            auto whileNode = std::make_unique<StructuredNode>(StructuredNodeKind::WHILE);
            
            // Get condition from branch statement
            const IRStatement* branchStmt = nullptr;
            for (const auto& stmt : block->getStatements()) {
                if (stmt->getKind() == IRStatementKind::BRANCH) {
                    branchStmt = stmt.get();
                    break;
                }
            }
            
            if (branchStmt) {
                whileNode->setCondition(branchStmt->getCondition());
            }
            
            // Note: Don't add the condition block itself, as it contains BRANCH/GOTO
            // statements that should not be generated. The condition is already extracted.
            
            // Create child node for loop body
            if (bodyBlock) {
                auto bodyNode = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
                bodyNode->addBlock(bodyBlock);
                whileNode->addChild(std::move(bodyNode));
                processed.insert(bodyBlock);
            }
            
            root->addChild(std::move(whileNode));
            processed.insert(block);
            continue;
        }
        
        // Try If-Then-Else
        if (matchIfThenElse(block, function, thenBlock, elseBlock, mergeBlock)) {
            auto ifNode = std::make_unique<StructuredNode>(StructuredNodeKind::IF_THEN_ELSE);
            
            // Get condition from branch statement
            const IRStatement* branchStmt = nullptr;
            for (const auto& stmt : block->getStatements()) {
                if (stmt->getKind() == IRStatementKind::BRANCH) {
                    branchStmt = stmt.get();
                    break;
                }
            }
            
            if (branchStmt) {
                ifNode->setCondition(branchStmt->getCondition());
            }
            
            // Note: Don't add the condition block itself, as it contains BRANCH/GOTO
            // statements that should not be generated. The condition is already extracted.
            
            // Create child nodes for then and else branches
            if (thenBlock) {
                auto thenNode = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
                thenNode->addBlock(thenBlock);
                ifNode->addChild(std::move(thenNode));
                processed.insert(thenBlock);
            }
            if (elseBlock) {
                auto elseNode = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
                elseNode->addBlock(elseBlock);
                ifNode->addChild(std::move(elseNode));
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
            
            // Note: Don't add the condition block itself, as it contains BRANCH/GOTO
            // statements that should not be generated. The condition is already extracted.
            
            // Create child node for then branch
            if (thenBlock) {
                auto thenNode = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
                thenNode->addBlock(thenBlock);
                ifNode->addChild(std::move(thenNode));
                processed.insert(thenBlock);
            }
            
            root->addChild(std::move(ifNode));
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
    // Both branches converge to a merge block (optional)
    
    const auto& successors = block->getSuccessors();
    if (successors.size() != 2) {
        return false;
    }
    
    // Check if block has statements
    const auto& statements = block->getStatements();
    if (statements.empty()) {
        return false;
    }
    
    // Look for a BRANCH statement
    const IRStatement* branchStmt = nullptr;
    for (const auto& stmt : statements) {
        if (stmt->getKind() == IRStatementKind::BRANCH) {
            branchStmt = stmt.get();
            break;
        }
    }
    
    if (!branchStmt) {
        return false;
    }
    
    // Get the target of the BRANCH statement (this is the "then" block)
    uint32_t branchTargetId = branchStmt->getTargetBlockId();
    
    // Get the two successors
    auto it = successors.begin();
    uint32_t succ1Id = *it;
    ++it;
    uint32_t succ2Id = *it;
    
    const auto* succ1 = getBlockById(succ1Id, function);
    const auto* succ2 = getBlockById(succ2Id, function);
    
    if (!succ1 || !succ2) {
        return false;
    }
    
    // The branch target is the "then" block, the other is the "else" block
    if (succ1Id == branchTargetId) {
        thenBlock = succ1;
        elseBlock = succ2;
    } else {
        thenBlock = succ2;
        elseBlock = succ1;
    }
    
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
    
    // Check if block has statements
    const auto& statements = block->getStatements();
    if (statements.empty()) {
        return false;
    }
    
    // Look for a BRANCH statement
    const IRStatement* branchStmt = nullptr;
    for (const auto& stmt : statements) {
        if (stmt->getKind() == IRStatementKind::BRANCH) {
            branchStmt = stmt.get();
            break;
        }
    }
    
    if (!branchStmt) {
        return false;
    }
    
    // Get the target of the BRANCH statement
    uint32_t branchTargetId = branchStmt->getTargetBlockId();
    
    // Get the two successors
    auto it = successors.begin();
    uint32_t succ1Id = *it;
    ++it;
    uint32_t succ2Id = *it;
    
    const auto* succ1 = getBlockById(succ1Id, function);
    const auto* succ2 = getBlockById(succ2Id, function);
    
    if (!succ1 || !succ2) {
        return false;
    }
    
    // Check if one successor loops back to this block (back edge)
    bool succ1IsBackEdge = isBackEdge(succ1, block, function);
    bool succ2IsBackEdge = isBackEdge(succ2, block, function);
    
    // The branch target should be the body (continue loop), the other is exit
    if (succ1Id == branchTargetId) {
        bodyBlock = succ1;
        exitBlock = succ2;
        return succ1IsBackEdge; // Only match if it's actually a loop
    } else {
        bodyBlock = succ2;
        exitBlock = succ1;
        return succ2IsBackEdge; // Only match if it's actually a loop
    }
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
    // A back edge is an edge where the destination (to) can reach back to itself
    // through the source (from). In simpler terms, if 'from' has 'to' as a successor,
    // and 'to' has 'from' as a predecessor, and 'to' is an ancestor of 'from' in the
    // dominator tree, it's a back edge.
    
    // Simplified approach: if 'from' points to 'to', and 'to' has a lower or equal ID
    // than 'from', it's likely a back edge (loop back to header).
    // This works for simple loops where the header comes before the body in block order.
    
    // Check if there's an edge from 'from' to 'to'
    const auto& fromSuccs = from->getSuccessors();
    bool hasEdge = false;
    for (uint32_t succId : fromSuccs) {
        if (succId == to->getId()) {
            hasEdge = true;
            break;
        }
    }
    
    if (!hasEdge) {
        return false;
    }
    
    // Check if this creates a cycle: does 'to' have 'from' in its descendants?
    // Simple heuristic: if 'to' has a lower ID, it's likely a back edge
    return to->getId() <= from->getId();
}

const IRBasicBlock* ControlFlowStructurer::getBlockById(uint32_t id, const IRFunction& function) const {
    return function.getBasicBlock(id);
}

} // namespace VBDecompiler
