// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#pragma once

#include "../ir/IRFunction.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <string>

namespace VBDecompiler {

/**
 * @brief Structured Control Flow Node Types
 */
enum class StructuredNodeKind {
    SEQUENCE,       // Sequential statements
    IF_THEN,        // If...Then (no else)
    IF_THEN_ELSE,   // If...Then...Else
    WHILE,          // While...Wend
    DO_WHILE,       // Do...Loop While
    DO_UNTIL,       // Do...Loop Until
    FOR,            // For...Next
    SELECT,         // Select Case
    GOTO_LABEL      // Goto/Label (fallback for irreducible control flow)
};

/**
 * @brief Structured Control Flow Node
 * 
 * Represents a high-level control structure recovered from the CFG.
 */
class StructuredNode {
public:
    explicit StructuredNode(StructuredNodeKind kind) : kind_(kind) {}
    virtual ~StructuredNode() = default;
    
    StructuredNodeKind getKind() const { return kind_; }
    
    // Child nodes
    void addChild(std::unique_ptr<StructuredNode> child) {
        children_.push_back(std::move(child));
    }
    
    const std::vector<std::unique_ptr<StructuredNode>>& getChildren() const {
        return children_;
    }
    
    // Associated IR basic blocks
    void addBlock(const IRBasicBlock* block) {
        blocks_.push_back(block);
    }
    
    const std::vector<const IRBasicBlock*>& getBlocks() const {
        return blocks_;
    }
    
    // Condition (for If/While/Do)
    void setCondition(const IRExpression* cond) {
        condition_ = cond;
    }
    
    const IRExpression* getCondition() const {
        return condition_;
    }

private:
    StructuredNodeKind kind_;
    std::vector<std::unique_ptr<StructuredNode>> children_;
    std::vector<const IRBasicBlock*> blocks_;  // IR blocks that make up this structure
    const IRExpression* condition_ = nullptr;  // For conditional structures
};

/**
 * @brief Control Flow Structurer
 * 
 * Converts low-level CFG (Control Flow Graph) to high-level structured
 * control flow (If/While/For/Select).
 * 
 * Uses structural analysis to identify control flow patterns:
 * - If-Then-Else (branch with merge)
 * - Loops (back edges in CFG)
 * - Sequential blocks
 * 
 * For irreducible control flow (goto spaghetti), falls back to explicit
 * goto/label statements.
 */
class ControlFlowStructurer {
public:
    ControlFlowStructurer() = default;
    
    /**
     * @brief Structure the control flow of an IR function
     * @param function IR function to structure
     * @return Root of structured control flow tree
     */
    std::unique_ptr<StructuredNode> structureFunction(const IRFunction& function);
    
    /**
     * @brief Check if CFG is reducible (can be structured)
     * @param function IR function to check
     * @return True if reducible, false if contains irreducible control flow
     */
    bool isReducible(const IRFunction& function) const;

private:
    // Structural analysis passes
    std::unique_ptr<StructuredNode> analyzeRegion(
        const std::vector<const IRBasicBlock*>& blocks,
        const IRFunction& function
    );
    
    // Pattern detection
    bool matchIfThenElse(
        const IRBasicBlock* block,
        const IRFunction& function,
        const IRBasicBlock*& thenBlock,
        const IRBasicBlock*& elseBlock,
        const IRBasicBlock*& mergeBlock
    );
    
    bool matchIfThen(
        const IRBasicBlock* block,
        const IRFunction& function,
        const IRBasicBlock*& thenBlock,
        const IRBasicBlock*& mergeBlock
    );
    
    bool matchWhileLoop(
        const IRBasicBlock* block,
        const IRFunction& function,
        const IRBasicBlock*& bodyBlock,
        const IRBasicBlock*& exitBlock
    );
    
    // CFG analysis helpers
    std::vector<const IRBasicBlock*> getBlocksInPostOrder(const IRFunction& function) const;
    std::unordered_set<const IRBasicBlock*> getDominators(
        const IRBasicBlock* block,
        const IRFunction& function
    ) const;
    
    bool isBackEdge(
        const IRBasicBlock* from,
        const IRBasicBlock* to,
        const IRFunction& function
    ) const;
    
    // Utility
    const IRBasicBlock* getBlockById(uint32_t id, const IRFunction& function) const;
};

} // namespace VBDecompiler
