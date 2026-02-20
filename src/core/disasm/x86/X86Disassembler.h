#ifndef X86DISASSEMBLER_H
#define X86DISASSEMBLER_H

#include "X86Instruction.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <span>

namespace VBDecompiler {

/**
 * @brief x86 Disassembler
 * 
 * Decodes x86 machine code into X86Instruction objects.
 * Supports 32-bit x86 (i386) instruction set.
 */
class X86Disassembler {
public:
    X86Disassembler() = default;
    
    /**
     * @brief Disassemble a single instruction at the given address
     * @param data Raw bytes to disassemble
     * @param address Virtual address of the instruction
     * @return Disassembled instruction, or nullptr if decoding failed
     */
    std::unique_ptr<X86Instruction> disassembleOne(std::span<const uint8_t> data, uint32_t address);
    
    /**
     * @brief Disassemble multiple instructions
     * @param data Raw bytes to disassemble
     * @param address Starting virtual address
     * @param count Maximum number of instructions to disassemble
     * @return Vector of disassembled instructions
     */
    std::vector<X86Instruction> disassemble(std::span<const uint8_t> data, 
                                            uint32_t address, 
                                            size_t count = 0);
    
    /**
     * @brief Disassemble a function
     * @param data Raw bytes containing the function
     * @param address Starting address of the function
     * @return Vector of disassembled instructions (stops at RET)
     */
    std::vector<X86Instruction> disassembleFunction(std::span<const uint8_t> data, 
                                                     uint32_t address);
    
    /**
     * @brief Get last error message
     */
    const std::string& getLastError() const { return lastError_; }

private:
    // Decoding helpers
    bool decodeModRM(std::span<const uint8_t> data, size_t& offset, 
                     uint8_t& mod, uint8_t& reg, uint8_t& rm);
    
    bool decodeSIB(std::span<const uint8_t> data, size_t& offset,
                   uint8_t& scale, uint8_t& index, uint8_t& base);
    
    X86Register getReg32(uint8_t regNum);
    X86Register getReg16(uint8_t regNum);
    X86Register getReg8(uint8_t regNum);
    
    bool decodeMemoryOperand(std::span<const uint8_t> data, size_t& offset,
                            uint8_t mod, uint8_t rm, uint8_t operandSize,
                            X86Operand& operand);
    
    // Helper to apply SIB byte to operand
    void applySIBToOperand(uint8_t scale, uint8_t index, uint8_t base, X86Operand& operand);
    
    // Operand creation helpers
    static X86Operand makeRegisterOperand(X86Register reg, uint8_t size);
    static X86Operand makeImmediateOperand(uint32_t value, uint8_t size);
    static X86Operand makeOffsetOperand(int32_t offset, uint8_t size);
    
    // High-level ModR/M decoding helpers
    bool decodeModRMtoRegOperands(std::span<const uint8_t> data, size_t& offset,
                                   uint8_t size, X86Operand& dst, X86Operand& src);
    bool decodeRegtoModRMOperands(std::span<const uint8_t> data, size_t& offset,
                                   uint8_t size, X86Operand& dst, X86Operand& src);
    
    // Instruction decoders for specific opcodes
    bool decodeMov(std::span<const uint8_t> data, size_t& offset, 
                   X86Instruction& instr, uint8_t opcode);
    bool decodePush(std::span<const uint8_t> data, size_t& offset,
                    X86Instruction& instr, uint8_t opcode);
    bool decodePop(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr, uint8_t opcode);
    bool decodeCall(std::span<const uint8_t> data, size_t& offset,
                    X86Instruction& instr, uint8_t opcode);
    bool decodeJmp(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr, uint8_t opcode);
    bool decodeJcc(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr, uint8_t opcode);
    bool decodeRet(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr, uint8_t opcode);
    bool decodeArithmetic(std::span<const uint8_t> data, size_t& offset,
                         X86Instruction& instr, uint8_t opcode);
    bool decodeLea(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr, uint8_t opcode);
    bool decodeTest(std::span<const uint8_t> data, size_t& offset,
                    X86Instruction& instr, uint8_t opcode);
    bool decodeXor(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr, uint8_t opcode);
    bool decodeAnd(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr, uint8_t opcode);
    bool decodeOr(std::span<const uint8_t> data, size_t& offset,
                  X86Instruction& instr, uint8_t opcode);
    bool decodeIncDec(std::span<const uint8_t> data, size_t& offset,
                      X86Instruction& instr, uint8_t opcode);
    bool decodeLeave(std::span<const uint8_t> data, size_t& offset,
                     X86Instruction& instr);
    bool decodeNop(std::span<const uint8_t> data, size_t& offset,
                   X86Instruction& instr);
    
    // Read helpers
    bool readByte(std::span<const uint8_t> data, size_t& offset, uint8_t& value);
    bool readWord(std::span<const uint8_t> data, size_t& offset, uint16_t& value);
    bool readDword(std::span<const uint8_t> data, size_t& offset, uint32_t& value);
    bool readSByte(std::span<const uint8_t> data, size_t& offset, int8_t& value);
    bool readSDword(std::span<const uint8_t> data, size_t& offset, int32_t& value);
    
    void setError(const std::string& error);
    
    std::string lastError_;
};

} // namespace VBDecompiler

#endif // X86DISASSEMBLER_H
