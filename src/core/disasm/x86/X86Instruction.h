#ifndef X86INSTRUCTION_H
#define X86INSTRUCTION_H

#include <cstdint>
#include <string>
#include <vector>

namespace VBDecompiler {

/**
 * @brief x86 Register enumeration
 */
enum class X86Register : uint8_t {
    // 8-bit registers
    AL, CL, DL, BL, AH, CH, DH, BH,
    
    // 16-bit registers
    AX, CX, DX, BX, SP, BP, SI, DI,
    
    // 32-bit registers
    EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
    
    // Segment registers
    ES, CS, SS, DS, FS, GS,
    
    // Special
    NONE
};

/**
 * @brief x86 Opcode enumeration
 */
enum class X86Opcode : uint16_t {
    // Data movement
    MOV,
    PUSH,
    POP,
    XCHG,
    LEA,
    
    // Arithmetic
    ADD,
    SUB,
    MUL,
    IMUL,
    DIV,
    IDIV,
    INC,
    DEC,
    NEG,
    
    // Logical
    AND,
    OR,
    XOR,
    NOT,
    TEST,
    CMP,
    
    // Bit operations
    SHL,
    SHR,
    SAL,
    SAR,
    ROL,
    ROR,
    
    // Control flow
    JMP,
    JE, JNE, JZ, JNZ,
    JA, JAE, JB, JBE,
    JG, JGE, JL, JLE,
    JO, JNO, JS, JNS,
    JP, JNP, JPE, JPO,
    JCXZ, JECXZ,
    CALL,
    RET,
    RETN,
    RETF,
    
    // Loop
    LOOP,
    LOOPE, LOOPZ,
    LOOPNE, LOOPNZ,
    
    // String operations
    MOVSB, MOVSW, MOVSD,
    CMPSB, CMPSW, CMPSD,
    STOSB, STOSW, STOSD,
    LODSB, LODSW, LODSD,
    SCASB, SCASW, SCASD,
    REP, REPE, REPZ, REPNE, REPNZ,
    
    // Stack frame
    ENTER,
    LEAVE,
    
    // Misc
    NOP,
    INT,
    INT3,
    HLT,
    
    // FPU (basic)
    FLD, FST, FSTP,
    FADD, FSUB, FMUL, FDIV,
    
    UNKNOWN
};

/**
 * @brief Operand type
 */
enum class X86OperandType : uint8_t {
    NONE,
    REGISTER,       // Register operand (e.g., eax)
    IMMEDIATE,      // Immediate value (e.g., 0x1234)
    MEMORY,         // Memory operand (e.g., [eax+4])
    OFFSET          // Relative offset for jumps/calls
};

/**
 * @brief x86 operand
 */
struct X86Operand {
    X86OperandType type = X86OperandType::NONE;
    
    // For REGISTER
    X86Register reg = X86Register::NONE;
    
    // For IMMEDIATE
    uint32_t immediate = 0;
    
    // For MEMORY: [base + index*scale + disp]
    X86Register base = X86Register::NONE;
    X86Register index = X86Register::NONE;
    uint8_t scale = 0;      // 1, 2, 4, or 8
    int32_t displacement = 0;
    
    // For OFFSET (jumps/calls)
    int32_t offset = 0;
    
    // Size in bytes
    uint8_t size = 0;  // 1, 2, or 4 bytes
    
    std::string toString(uint32_t instrAddress = 0, uint8_t instrLength = 0) const;
};

/**
 * @brief x86 instruction
 */
class X86Instruction {
public:
    X86Instruction() = default;
    
    /**
     * @brief Get the opcode
     */
    X86Opcode getOpcode() const { return opcode_; }
    
    /**
     * @brief Set the opcode
     */
    void setOpcode(X86Opcode opcode) { opcode_ = opcode; }
    
    /**
     * @brief Get opcode mnemonic as string
     */
    std::string getMnemonic() const;
    
    /**
     * @brief Get the address of this instruction
     */
    uint32_t getAddress() const { return address_; }
    
    /**
     * @brief Set the address
     */
    void setAddress(uint32_t address) { address_ = address; }
    
    /**
     * @brief Get instruction length in bytes
     */
    uint8_t getLength() const { return length_; }
    
    /**
     * @brief Set instruction length
     */
    void setLength(uint8_t length) { length_ = length; }
    
    /**
     * @brief Get raw bytes
     */
    const std::vector<uint8_t>& getBytes() const { return bytes_; }
    
    /**
     * @brief Set raw bytes
     */
    void setBytes(const std::vector<uint8_t>& bytes) { bytes_ = bytes; }
    
    /**
     * @brief Get operands
     */
    const std::vector<X86Operand>& getOperands() const { return operands_; }
    
    /**
     * @brief Add an operand
     */
    void addOperand(const X86Operand& operand) { operands_.push_back(operand); }
    
    /**
     * @brief Check if this is a branch instruction
     */
    bool isBranch() const;
    
    /**
     * @brief Check if this is a conditional branch
     */
    bool isConditionalBranch() const;
    
    /**
     * @brief Check if this is a call instruction
     */
    bool isCall() const;
    
    /**
     * @brief Check if this is a return instruction
     */
    bool isReturn() const;
    
    /**
     * @brief Get branch target address (if branch/call)
     */
    uint32_t getBranchTarget() const;
    
    /**
     * @brief Format as string: "mnemonic operand1, operand2"
     */
    std::string toString() const;
    
    /**
     * @brief Format bytes as hex string
     */
    std::string getBytesString() const;

private:
    uint32_t address_ = 0;
    X86Opcode opcode_ = X86Opcode::UNKNOWN;
    uint8_t length_ = 0;
    std::vector<uint8_t> bytes_;
    std::vector<X86Operand> operands_;
};

} // namespace VBDecompiler

#endif // X86INSTRUCTION_H
