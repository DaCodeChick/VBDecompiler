// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

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
    
    // Get all blocks in execution order using BFS
    std::vector<const IRBasicBlock*> blocks;
    std::unordered_set<uint32_t> visitedIds;  // Use IDs instead of pointers
    std::queue<const IRBasicBlock*> queue;
    
    queue.push(entryBlock);
    visitedIds.insert(entryBlock->getId());
    
    while (!queue.empty()) {
        const auto* block = queue.front();
        queue.pop();
        blocks.push_back(block);
        
        // Add successors
        for (uint32_t succId : block->getSuccessors()) {
            if (!visitedIds.contains(succId)) {
                visitedIds.insert(succId);
                const auto* succBlock = getBlockById(succId, function);
                if (succBlock) {
                    queue.push(succBlock);
                }
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
    
    // Process blocks in order - use IDs instead of pointers for tracking
    std::unordered_set<uint32_t> processedIds;
    
    for (const auto* block : blocks) {
        if (processedIds.contains(block->getId())) {
            continue;
        }
        
        // Try to match control flow patterns
        const IRBasicBlock* thenBlock = nullptr;
        const IRBasicBlock* elseBlock = nullptr;
        const IRBasicBlock* mergeBlock = nullptr;
        const IRBasicBlock* bodyBlock = nullptr;
        const IRBasicBlock* exitBlock = nullptr;
        
        // Try Do-While loop first (self-loop pattern)
        // Must check before While loop since both have back edges
        if (matchDoWhileLoop(block, function, exitBlock)) {
            auto doWhileNode = std::make_unique<StructuredNode>(StructuredNodeKind::DO_WHILE);
            
            // Get condition from branch statement (last statement in body)
            const IRStatement* branchStmt = nullptr;
            for (const auto& stmt : block->getStatements()) {
                if (stmt->getKind() == IRStatementKind::BRANCH) {
                    branchStmt = stmt.get();
                    break;
                }
            }
            
            if (branchStmt) {
                doWhileNode->setCondition(branchStmt->getCondition());
            }
            
            // Add the body block to a SEQUENCE node
            // The loop body includes all statements except the condition check
            auto bodyNode = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
            bodyNode->addBlock(block);
            doWhileNode->addChild(std::move(bodyNode));
            
            root->addChild(std::move(doWhileNode));
            processedIds.insert(block->getId());
            continue;
        }
        
        // Try While loop (separate header and body blocks)
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
            
            // Recursively structure loop body
            if (bodyBlock) {
                // Collect all blocks in the loop body (from bodyBlock until exitBlock)
                // Exclude the condition block (current block) to prevent infinite recursion
                auto bodyRegionBlocks = collectRegionBlocks(bodyBlock, exitBlock, block, function);
                
                // Recursively structure the loop body
                auto bodyNode = analyzeRegion(bodyRegionBlocks, function);
                if (bodyNode) {
                    whileNode->addChild(std::move(bodyNode));
                }
                
                // Mark all blocks in the loop body as processed
                for (const auto* b : bodyRegionBlocks) {
                    processedIds.insert(b->getId());
                }
            }
            
            root->addChild(std::move(whileNode));
            processedIds.insert(block->getId());
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
            
            // Recursively structure then and else branches
            if (thenBlock) {
                // Collect all blocks in the then region (from thenBlock until mergeBlock)
                // Exclude the condition block to avoid including it in the branch
                auto thenRegionBlocks = collectRegionBlocks(thenBlock, mergeBlock, block, function);
                
                // Recursively structure the then region
                auto thenNode = analyzeRegion(thenRegionBlocks, function);
                if (thenNode) {
                    ifNode->addChild(std::move(thenNode));
                }
                
                // Mark all blocks in the then region as processed
                for (const auto* b : thenRegionBlocks) {
                    processedIds.insert(b->getId());
                }
            }
            
            if (elseBlock) {
                // Collect all blocks in the else region (from elseBlock until mergeBlock)
                // Exclude the condition block to avoid including it in the branch
                auto elseRegionBlocks = collectRegionBlocks(elseBlock, mergeBlock, block, function);
                
                // Recursively structure the else region
                auto elseNode = analyzeRegion(elseRegionBlocks, function);
                if (elseNode) {
                    ifNode->addChild(std::move(elseNode));
                }
                
                // Mark all blocks in the else region as processed
                for (const auto* b : elseRegionBlocks) {
                    processedIds.insert(b->getId());
                }
            }
            
            root->addChild(std::move(ifNode));
            processedIds.insert(block->getId());
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
            
            // Recursively structure then branch
            if (thenBlock) {
                // Collect all blocks in the then region (from thenBlock until mergeBlock)
                // Exclude the condition block to avoid including it in the branch
                auto thenRegionBlocks = collectRegionBlocks(thenBlock, mergeBlock, block, function);
                
                // Recursively structure the then region
                auto thenNode = analyzeRegion(thenRegionBlocks, function);
                if (thenNode) {
                    ifNode->addChild(std::move(thenNode));
                }
                
                // Mark all blocks in the then region as processed
                for (const auto* b : thenRegionBlocks) {
                    processedIds.insert(b->getId());
                }
            }
            
            root->addChild(std::move(ifNode));
            processedIds.insert(block->getId());
            continue;
        }
        
        // Default: sequential block
        auto seqNode = std::make_unique<StructuredNode>(StructuredNodeKind::SEQUENCE);
        seqNode->addBlock(block);
        root->addChild(std::move(seqNode));
        processedIds.insert(block->getId());
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
    const IRBasicBlock* candidateThen = nullptr;
    const IRBasicBlock* candidateElse = nullptr;
    
    if (succ1Id == branchTargetId) {
        candidateThen = succ1;
        candidateElse = succ2;
    } else {
        candidateThen = succ2;
        candidateElse = succ1;
    }
    
    // Key insight: If the then block's ONLY successor is the "else" block,
    // then the "else" block is actually the merge point, not an else branch.
    // This is an If-Then pattern, not If-Then-Else.
    const auto& thenSuccs = candidateThen->getSuccessors();
    if (thenSuccs.size() == 1 && thenSuccs.contains(candidateElse->getId())) {
        // Then block jumps directly to "else" block - this is If-Then
        return false;
    }
    
    // Try to find merge block (common successor)
    const auto& elseSuccs = candidateElse->getSuccessors();
    
    for (uint32_t thenSuccId : thenSuccs) {
        for (uint32_t elseSuccId : elseSuccs) {
            if (thenSuccId == elseSuccId) {
                // Found a common successor - this is the merge block
                thenBlock = candidateThen;
                elseBlock = candidateElse;
                mergeBlock = getBlockById(thenSuccId, function);
                return true;
            }
        }
    }
    
    // No merge block found - this is still valid If-Then-Else if both branches
    // have meaningful content (e.g., both return or exit differently)
    // Accept it as If-Then-Else with no merge
    thenBlock = candidateThen;
    elseBlock = candidateElse;
    mergeBlock = nullptr;
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
    
    // Check if block has statements
    if (block->getStatements().empty()) {
        return false;
    }
    
    // Look for BRANCH statement to determine which successor is the then block
    const IRStatement* branchStmt = nullptr;
    for (const auto& stmt : block->getStatements()) {
        if (stmt->getKind() == IRStatementKind::BRANCH) {
            branchStmt = stmt.get();
            break;
        }
    }
    
    if (!branchStmt) {
        return false;
    }
    
    // Get branch target (this is the then block)
    uint32_t branchTargetId = branchStmt->getTargetBlockId();
    
    auto it = successors.begin();
    uint32_t succ1Id = *it;
    ++it;
    uint32_t succ2Id = *it;
    
    const auto* succ1 = getBlockById(succ1Id, function);
    const auto* succ2 = getBlockById(succ2Id, function);
    
    if (!succ1 || !succ2) {
        return false;
    }
    
    // Branch target is then block, the other is the merge/fallthrough
    if (succ1Id == branchTargetId) {
        thenBlock = succ1;
        mergeBlock = succ2;
    } else {
        thenBlock = succ2;
        mergeBlock = succ1;
    }
    
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

bool ControlFlowStructurer::matchDoWhileLoop(
    const IRBasicBlock* block,
    const IRFunction& function,
    const IRBasicBlock*& exitBlock
) {
    // Pattern for Do-While: 
    // - Block has 2 successors
    // - One successor is the block itself (back edge - loop continues)
    // - Other successor is the exit block
    // - Block has a BRANCH statement that tests the loop condition
    
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
    
    // Check if one successor is the block itself (loop back to same block)
    bool succ1IsLoopBack = (succ1Id == block->getId());
    bool succ2IsLoopBack = (succ2Id == block->getId());
    
    if (!succ1IsLoopBack && !succ2IsLoopBack) {
        return false; // Not a Do-While if there's no self-loop
    }
    
    // Determine which successor is the exit
    // The branch target should loop back, the fallthrough should exit
    if (succ1Id == branchTargetId && succ1IsLoopBack) {
        exitBlock = succ2;
        return true;
    } else if (succ2Id == branchTargetId && succ2IsLoopBack) {
        exitBlock = succ1;
        return true;
    }
    
    return false;
}

std::vector<const IRBasicBlock*> ControlFlowStructurer::getBlocksInPostOrder(const IRFunction& function) const {
    std::vector<const IRBasicBlock*> result;
    std::unordered_set<uint32_t> visitedIds;
    std::stack<const IRBasicBlock*> stack;
    
    const auto* entryBlock = function.getEntryBlock();
    if (!entryBlock) {
        return result;
    }
    
    stack.push(entryBlock);
    
    while (!stack.empty()) {
        const auto* block = stack.top();
        
        if (visitedIds.contains(block->getId())) {
            stack.pop();
            result.push_back(block);
            continue;
        }
        
        visitedIds.insert(block->getId());
        
        // Push successors
        for (uint32_t succId : block->getSuccessors()) {
            if (!visitedIds.contains(succId)) {
                const auto* succBlock = getBlockById(succId, function);
                if (succBlock) {
                    stack.push(succBlock);
                }
            }
        }
    }
    
    return result;
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

std::vector<const IRBasicBlock*> ControlFlowStructurer::collectRegionBlocks(
    const IRBasicBlock* startBlock,
    const IRBasicBlock* exitBlock,
    const IRBasicBlock* excludeBlock,
    const IRFunction& function
) const {
    if (!startBlock) {
        return {};
    }
    
    std::vector<const IRBasicBlock*> regionBlocks;
    std::unordered_set<uint32_t> visitedIds;  // Use IDs instead of pointers
    std::queue<const IRBasicBlock*> queue;
    
    uint32_t exitId = exitBlock ? exitBlock->getId() : UINT32_MAX;
    uint32_t excludeId = excludeBlock ? excludeBlock->getId() : UINT32_MAX;
    
    queue.push(startBlock);
    visitedIds.insert(startBlock->getId());
    
    while (!queue.empty()) {
        const auto* block = queue.front();
        queue.pop();
        
        uint32_t blockId = block->getId();
        
        // Don't include the exit block or exclude block
        if (blockId == exitId || blockId == excludeId) {
            continue;
        }
        
        regionBlocks.push_back(block);
        
        // Add successors to queue (but only if they're not the exit or exclude block)
        for (uint32_t succId : block->getSuccessors()) {
            // Skip if already visited
            if (visitedIds.contains(succId)) {
                continue;
            }
            
            visitedIds.insert(succId);
            
            // Don't traverse past the exit block or exclude block
            if (succId != exitId && succId != excludeId) {
                const auto* succBlock = getBlockById(succId, function);
                if (succBlock) {
                    queue.push(succBlock);
                }
            }
        }
    }
    
    return regionBlocks;
}

} // namespace VBDecompiler
