#include "X86Instruction.h"
#include <sstream>
#include <iomanip>
#include <unordered_map>

namespace VBDecompiler {

// Register name lookup tables
static const std::unordered_map<X86Register, std::string> registerNames = {
    // 8-bit
    {X86Register::AL, "al"}, {X86Register::CL, "cl"}, {X86Register::DL, "dl"}, {X86Register::BL, "bl"},
    {X86Register::AH, "ah"}, {X86Register::CH, "ch"}, {X86Register::DH, "dh"}, {X86Register::BH, "bh"},
    
    // 16-bit
    {X86Register::AX, "ax"}, {X86Register::CX, "cx"}, {X86Register::DX, "dx"}, {X86Register::BX, "bx"},
    {X86Register::SP, "sp"}, {X86Register::BP, "bp"}, {X86Register::SI, "si"}, {X86Register::DI, "di"},
    
    // 32-bit
    {X86Register::EAX, "eax"}, {X86Register::ECX, "ecx"}, {X86Register::EDX, "edx"}, {X86Register::EBX, "ebx"},
    {X86Register::ESP, "esp"}, {X86Register::EBP, "ebp"}, {X86Register::ESI, "esi"}, {X86Register::EDI, "edi"},
    
    // Segment
    {X86Register::ES, "es"}, {X86Register::CS, "cs"}, {X86Register::SS, "ss"},
    {X86Register::DS, "ds"}, {X86Register::FS, "fs"}, {X86Register::GS, "gs"},
};

// Opcode mnemonic lookup table
static const std::unordered_map<X86Opcode, std::string> opcodeMnemonics = {
    {X86Opcode::MOV, "mov"}, {X86Opcode::PUSH, "push"}, {X86Opcode::POP, "pop"},
    {X86Opcode::XCHG, "xchg"}, {X86Opcode::LEA, "lea"},
    
    {X86Opcode::ADD, "add"}, {X86Opcode::SUB, "sub"}, {X86Opcode::MUL, "mul"},
    {X86Opcode::IMUL, "imul"}, {X86Opcode::DIV, "div"}, {X86Opcode::IDIV, "idiv"},
    {X86Opcode::INC, "inc"}, {X86Opcode::DEC, "dec"}, {X86Opcode::NEG, "neg"},
    
    {X86Opcode::AND, "and"}, {X86Opcode::OR, "or"}, {X86Opcode::XOR, "xor"},
    {X86Opcode::NOT, "not"}, {X86Opcode::TEST, "test"}, {X86Opcode::CMP, "cmp"},
    
    {X86Opcode::SHL, "shl"}, {X86Opcode::SHR, "shr"}, {X86Opcode::SAL, "sal"},
    {X86Opcode::SAR, "sar"}, {X86Opcode::ROL, "rol"}, {X86Opcode::ROR, "ror"},
    
    {X86Opcode::JMP, "jmp"},
    {X86Opcode::JE, "je"}, {X86Opcode::JNE, "jne"}, {X86Opcode::JZ, "jz"}, {X86Opcode::JNZ, "jnz"},
    {X86Opcode::JA, "ja"}, {X86Opcode::JAE, "jae"}, {X86Opcode::JB, "jb"}, {X86Opcode::JBE, "jbe"},
    {X86Opcode::JG, "jg"}, {X86Opcode::JGE, "jge"}, {X86Opcode::JL, "jl"}, {X86Opcode::JLE, "jle"},
    {X86Opcode::JO, "jo"}, {X86Opcode::JNO, "jno"}, {X86Opcode::JS, "js"}, {X86Opcode::JNS, "jns"},
    {X86Opcode::JP, "jp"}, {X86Opcode::JNP, "jnp"}, {X86Opcode::JPE, "jpe"}, {X86Opcode::JPO, "jpo"},
    {X86Opcode::JCXZ, "jcxz"}, {X86Opcode::JECXZ, "jecxz"},
    {X86Opcode::CALL, "call"},
    {X86Opcode::RET, "ret"}, {X86Opcode::RETN, "retn"}, {X86Opcode::RETF, "retf"},
    
    {X86Opcode::LOOP, "loop"}, {X86Opcode::LOOPE, "loope"}, {X86Opcode::LOOPZ, "loopz"},
    {X86Opcode::LOOPNE, "loopne"}, {X86Opcode::LOOPNZ, "loopnz"},
    
    {X86Opcode::MOVSB, "movsb"}, {X86Opcode::MOVSW, "movsw"}, {X86Opcode::MOVSD, "movsd"},
    {X86Opcode::CMPSB, "cmpsb"}, {X86Opcode::CMPSW, "cmpsw"}, {X86Opcode::CMPSD, "cmpsd"},
    {X86Opcode::STOSB, "stosb"}, {X86Opcode::STOSW, "stosw"}, {X86Opcode::STOSD, "stosd"},
    {X86Opcode::LODSB, "lodsb"}, {X86Opcode::LODSW, "lodsw"}, {X86Opcode::LODSD, "lodsd"},
    {X86Opcode::SCASB, "scasb"}, {X86Opcode::SCASW, "scasw"}, {X86Opcode::SCASD, "scasd"},
    {X86Opcode::REP, "rep"}, {X86Opcode::REPE, "repe"}, {X86Opcode::REPZ, "repz"},
    {X86Opcode::REPNE, "repne"}, {X86Opcode::REPNZ, "repnz"},
    
    {X86Opcode::ENTER, "enter"}, {X86Opcode::LEAVE, "leave"},
    
    {X86Opcode::NOP, "nop"}, {X86Opcode::INT, "int"}, {X86Opcode::INT3, "int3"}, {X86Opcode::HLT, "hlt"},
    
    {X86Opcode::FLD, "fld"}, {X86Opcode::FST, "fst"}, {X86Opcode::FSTP, "fstp"},
    {X86Opcode::FADD, "fadd"}, {X86Opcode::FSUB, "fsub"}, {X86Opcode::FMUL, "fmul"}, {X86Opcode::FDIV, "fdiv"},
    
    {X86Opcode::UNKNOWN, "???"},
};

std::string X86Operand::toString(uint32_t instrAddress, uint8_t instrLength) const {
    std::ostringstream ss;
    
    switch (type) {
        case X86OperandType::REGISTER:
            if (registerNames.count(reg)) {
                ss << registerNames.at(reg);
            }
            break;
            
        case X86OperandType::IMMEDIATE:
            ss << "0x" << std::hex << std::uppercase << immediate;
            break;
            
        case X86OperandType::MEMORY: {
            ss << (size == 1 ? "byte" : size == 2 ? "word" : "dword") << " ptr [";
            
            bool needPlus = false;
            if (base != X86Register::NONE) {
                ss << registerNames.at(base);
                needPlus = true;
            }
            
            if (index != X86Register::NONE) {
                if (needPlus) ss << "+";
                ss << registerNames.at(index);
                if (scale > 1) ss << "*" << (int)scale;
                needPlus = true;
            }
            
            if (displacement != 0 || !needPlus) {
                if (displacement > 0 && needPlus) ss << "+";
                if (displacement < 0) ss << "-";
                if (displacement != 0 || !needPlus) {
                    ss << "0x" << std::hex << std::uppercase << std::abs(displacement);
                }
            }
            
            ss << "]";
            break;
        }
            
        case X86OperandType::OFFSET:
            ss << "0x" << std::hex << std::uppercase << (instrAddress + instrLength + offset);
            break;
            
        case X86OperandType::NONE:
        default:
            break;
    }
    
    return ss.str();
}

std::string X86Instruction::getMnemonic() const {
    if (opcodeMnemonics.count(opcode_)) {
        return opcodeMnemonics.at(opcode_);
    }
    return "???";
}

bool X86Instruction::isBranch() const {
    return opcode_ == X86Opcode::JMP ||
           opcode_ == X86Opcode::JE || opcode_ == X86Opcode::JNE ||
           opcode_ == X86Opcode::JZ || opcode_ == X86Opcode::JNZ ||
           opcode_ == X86Opcode::JA || opcode_ == X86Opcode::JAE ||
           opcode_ == X86Opcode::JB || opcode_ == X86Opcode::JBE ||
           opcode_ == X86Opcode::JG || opcode_ == X86Opcode::JGE ||
           opcode_ == X86Opcode::JL || opcode_ == X86Opcode::JLE ||
           opcode_ == X86Opcode::JO || opcode_ == X86Opcode::JNO ||
           opcode_ == X86Opcode::JS || opcode_ == X86Opcode::JNS ||
           opcode_ == X86Opcode::JP || opcode_ == X86Opcode::JNP ||
           opcode_ == X86Opcode::JPE || opcode_ == X86Opcode::JPO ||
           opcode_ == X86Opcode::JCXZ || opcode_ == X86Opcode::JECXZ ||
           opcode_ == X86Opcode::LOOP || opcode_ == X86Opcode::LOOPE ||
           opcode_ == X86Opcode::LOOPZ || opcode_ == X86Opcode::LOOPNE ||
           opcode_ == X86Opcode::LOOPNZ;
}

bool X86Instruction::isConditionalBranch() const {
    return isBranch() && opcode_ != X86Opcode::JMP;
}

bool X86Instruction::isCall() const {
    return opcode_ == X86Opcode::CALL;
}

bool X86Instruction::isReturn() const {
    return opcode_ == X86Opcode::RET ||
           opcode_ == X86Opcode::RETN ||
           opcode_ == X86Opcode::RETF;
}

uint32_t X86Instruction::getBranchTarget() const {
    if ((isBranch() || isCall()) && !operands_.empty()) {
        const auto& operand = operands_[0];
        if (operand.type == X86OperandType::OFFSET) {
            return address_ + length_ + operand.offset;
        }
    }
    return 0;
}

std::string X86Instruction::toString() const {
    std::ostringstream ss;
    ss << getMnemonic();
    
    for (size_t i = 0; i < operands_.size(); ++i) {
        if (i == 0) ss << " ";
        else ss << ", ";
        ss << operands_[i].toString(address_, length_);
    }
    
    return ss.str();
}

std::string X86Instruction::getBytesString() const {
    std::ostringstream ss;
    for (size_t i = 0; i < bytes_.size(); ++i) {
        if (i > 0) ss << " ";
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << (int)bytes_[i];
    }
    return ss.str();
}

} // namespace VBDecompiler
