// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2024 VBDecompiler Project
// SPDX-License-Identifier: MIT

#ifndef IRFUNCTION_H
#define IRFUNCTION_H

#include "IRStatement.h"
#include "IRType.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace VBDecompiler {

/**
 * @brief IR Basic Block - Represents a basic block in the control flow graph
 * 
 * A basic block is a sequence of statements with:
 * - Single entry point (first statement)
 * - Single exit point (last statement)
 * - No branches except at the end
 */
class IRBasicBlock {
public:
    explicit IRBasicBlock(uint32_t id) : id_(id) {}
    
    // Basic block ID
    [[nodiscard]] uint32_t getId() const { return id_; }
    
    // Statements
    void addStatement(std::unique_ptr<IRStatement> stmt) {
        statements_.push_back(std::move(stmt));
    }
    
    [[nodiscard]] const std::vector<std::unique_ptr<IRStatement>>& getStatements() const {
        return statements_;
    }
    
    [[nodiscard]] std::vector<std::unique_ptr<IRStatement>>& getStatements() {
        return statements_;
    }
    
    [[nodiscard]] size_t getStatementCount() const { return statements_.size(); }
    [[nodiscard]] bool isEmpty() const { return statements_.empty(); }
    
    // Predecessors (blocks that can jump to this block)
    void addPredecessor(uint32_t blockId) {
        predecessors_.insert(blockId);
    }
    
    void removePredecessor(uint32_t blockId) {
        predecessors_.erase(blockId);
    }
    
    [[nodiscard]] const std::unordered_set<uint32_t>& getPredecessors() const {
        return predecessors_;
    }
    
    // Successors (blocks this block can jump to)
    void addSuccessor(uint32_t blockId) {
        successors_.insert(blockId);
    }
    
    void removeSuccessor(uint32_t blockId) {
        successors_.erase(blockId);
    }
    
    [[nodiscard]] const std::unordered_set<uint32_t>& getSuccessors() const {
        return successors_;
    }
    
    /**
     * @brief Check if this block is an entry block (no predecessors)
     */
    [[nodiscard]] bool isEntry() const { return predecessors_.empty(); }
    
    /**
     * @brief Check if this block is an exit block (no successors)
     */
    [[nodiscard]] bool isExit() const { return successors_.empty(); }
    
    /**
     * @brief Convert block to string representation
     */
    [[nodiscard]] std::string toString() const;
    
    // Disable copying
    IRBasicBlock(const IRBasicBlock&) = delete;
    IRBasicBlock& operator=(const IRBasicBlock&) = delete;
    
    // Enable moving
    IRBasicBlock(IRBasicBlock&&) noexcept = default;
    IRBasicBlock& operator=(IRBasicBlock&&) noexcept = default;
    
    ~IRBasicBlock() = default;

private:
    uint32_t id_;
    std::vector<std::unique_ptr<IRStatement>> statements_;
    std::unordered_set<uint32_t> predecessors_;  // Incoming edges
    std::unordered_set<uint32_t> successors_;    // Outgoing edges
};

/**
 * @brief IR Function - Represents a function in the IR
 * 
 * A function contains:
 * - Function name and signature (parameters, return type)
 * - Control Flow Graph (CFG) of basic blocks
 * - Local variables
 * - Entry and exit blocks
 */
class IRFunction {
public:
    explicit IRFunction(std::string name, IRType returnType)
        : name_(std::move(name))
        , returnType_(std::move(returnType))
        , nextBlockId_(0)
        , nextVariableId_(0)
        , entryBlockId_(0)
    {
    }
    
    // Function properties
    [[nodiscard]] const std::string& getName() const { return name_; }
    [[nodiscard]] const IRType& getReturnType() const { return returnType_; }
    
    void setAddress(uint32_t address) { address_ = address; }
    [[nodiscard]] uint32_t getAddress() const { return address_; }
    
    // Parameters
    void addParameter(IRVariable param) {
        parameters_.push_back(std::move(param));
    }
    
    [[nodiscard]] const std::vector<IRVariable>& getParameters() const {
        return parameters_;
    }
    
    [[nodiscard]] size_t getParameterCount() const { return parameters_.size(); }
    
    // Local variables
    IRVariable& createLocalVariable(std::string name, IRType type) {
        uint32_t id = nextVariableId_++;
        localVariables_.emplace_back(id, std::move(name), std::move(type));
        return localVariables_.back();
    }
    
    [[nodiscard]] const std::vector<IRVariable>& getLocalVariables() const {
        return localVariables_;
    }
    
    // Basic blocks
    IRBasicBlock& createBasicBlock() {
        uint32_t id = nextBlockId_++;
        auto block = std::make_unique<IRBasicBlock>(id);
        IRBasicBlock* blockPtr = block.get();
        blocks_[id] = std::move(block);
        return *blockPtr;
    }
    
    IRBasicBlock* getBasicBlock(uint32_t id) {
        auto it = blocks_.find(id);
        return (it != blocks_.end()) ? it->second.get() : nullptr;
    }
    
    [[nodiscard]] const IRBasicBlock* getBasicBlock(uint32_t id) const {
        auto it = blocks_.find(id);
        return (it != blocks_.end()) ? it->second.get() : nullptr;
    }
    
    [[nodiscard]] const std::unordered_map<uint32_t, std::unique_ptr<IRBasicBlock>>& getBasicBlocks() const {
        return blocks_;
    }
    
    [[nodiscard]] size_t getBasicBlockCount() const { return blocks_.size(); }
    
    // Entry/exit blocks
    void setEntryBlock(uint32_t blockId) { entryBlockId_ = blockId; }
    [[nodiscard]] uint32_t getEntryBlockId() const { return entryBlockId_; }
    
    [[nodiscard]] IRBasicBlock* getEntryBlock() { return getBasicBlock(entryBlockId_); }
    [[nodiscard]] const IRBasicBlock* getEntryBlock() const { return getBasicBlock(entryBlockId_); }
    
    /**
     * @brief Get all exit blocks (blocks with no successors)
     */
    [[nodiscard]] std::vector<const IRBasicBlock*> getExitBlocks() const {
        std::vector<const IRBasicBlock*> exitBlocks;
        for (const auto& [id, block] : blocks_) {
            if (block->isExit()) {
                exitBlocks.push_back(block.get());
            }
        }
        return exitBlocks;
    }
    
    /**
     * @brief Connect two basic blocks (add edge in CFG)
     */
    void connectBlocks(uint32_t fromId, uint32_t toId) {
        auto* fromBlock = getBasicBlock(fromId);
        auto* toBlock = getBasicBlock(toId);
        if (fromBlock && toBlock) {
            fromBlock->addSuccessor(toId);
            toBlock->addPredecessor(fromId);
        }
    }
    
    /**
     * @brief Convert function to string representation
     */
    [[nodiscard]] std::string toString() const;
    
    // Disable copying
    IRFunction(const IRFunction&) = delete;
    IRFunction& operator=(const IRFunction&) = delete;
    
    // Enable moving
    IRFunction(IRFunction&&) noexcept = default;
    IRFunction& operator=(IRFunction&&) noexcept = default;
    
    ~IRFunction() = default;

private:
    std::string name_;
    IRType returnType_;
    uint32_t address_ = 0;
    
    std::vector<IRVariable> parameters_;
    std::vector<IRVariable> localVariables_;
    
    std::unordered_map<uint32_t, std::unique_ptr<IRBasicBlock>> blocks_;
    uint32_t nextBlockId_;
    uint32_t nextVariableId_;
    uint32_t entryBlockId_;
};

} // namespace VBDecompiler

#endif // IRFUNCTION_H
