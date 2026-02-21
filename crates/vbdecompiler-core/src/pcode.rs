// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! P-Code disassembly module
//!
//! Decodes Visual Basic P-Code (bytecode) into instruction representations.
//! P-Code is a stack-based bytecode format with variable-length instructions.

use crate::error::{Error, Result};
use std::fmt;

/// P-Code opcode category
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OpcodeCategory {
    ControlFlow, // Branch, return, exit
    Stack,       // Push/pop literals and values
    Variable,    // Load/store variables
    Call,        // Function/method calls
    String,      // String operations
    Array,       // Array operations
    Loop,        // For/Next loops
    Memory,      // Memory management
    Arithmetic,  // Math operations
    Logical,     // Logical operations
    Comparison,  // Comparison operations
    Conversion,  // Type conversion
    Unknown,     // Unknown/unimplemented
}

/// P-Code data type specifier
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PCodeType {
    Unknown,
    Byte,    // b
    Boolean, // ?
    Integer, // % (2 bytes)
    Long,    // & (4 bytes)
    Single,  // ! (4 bytes float)
    Variant, // ~ (Variant type)
    String,  // z (String)
    Object,  // Object reference
}

impl PCodeType {
    /// Convert type to string representation
    pub fn to_string(&self) -> &'static str {
        match self {
            Self::Byte => "Byte",
            Self::Boolean => "Boolean",
            Self::Integer => "Integer",
            Self::Long => "Long",
            Self::Single => "Single",
            Self::Variant => "Variant",
            Self::String => "String",
            Self::Object => "Object",
            Self::Unknown => "Unknown",
        }
    }
}

/// P-Code operand value
#[derive(Debug, Clone)]
pub enum OperandValue {
    None,
    Byte(u8),
    Int16(i16),
    Int32(i32),
    Float(f32),
    String(String),
}

impl fmt::Display for OperandValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::None => write!(f, ""),
            Self::Byte(v) => write!(f, "0x{:02X}", v),
            Self::Int16(v) => write!(f, "{}", v),
            Self::Int32(v) => write!(f, "{}", v),
            Self::Float(v) => write!(f, "{}", v),
            Self::String(s) => write!(f, "\"{}\"", s),
        }
    }
}

/// P-Code instruction operand
#[derive(Debug, Clone)]
pub struct Operand {
    pub value: OperandValue,
    pub data_type: PCodeType,
}

impl Operand {
    fn new(value: OperandValue, data_type: PCodeType) -> Self {
        Self { value, data_type }
    }
}

/// P-Code instruction representation
#[derive(Debug, Clone)]
pub struct Instruction {
    pub address: u32,
    pub opcode: u8,
    pub extended_opcode: Option<u8>,
    pub mnemonic: String,
    pub operands: Vec<Operand>,
    pub bytes: Vec<u8>,
    pub category: OpcodeCategory,
    pub stack_delta: i32,
    pub is_branch: bool,
    pub is_conditional_branch: bool,
    pub is_call: bool,
    pub is_return: bool,
    pub branch_offset: Option<i32>,
}

impl Instruction {
    /// Create a new instruction
    fn new(address: u32, opcode: u8) -> Self {
        Self {
            address,
            opcode,
            extended_opcode: None,
            mnemonic: String::new(),
            operands: Vec::new(),
            bytes: Vec::new(),
            category: OpcodeCategory::Unknown,
            stack_delta: 0,
            is_branch: false,
            is_conditional_branch: false,
            is_call: false,
            is_return: false,
            branch_offset: None,
        }
    }

    /// Format instruction as assembly-like string
    pub fn to_string(&self) -> String {
        let operands_str = self
            .operands
            .iter()
            .map(|op| format!("{}", op.value))
            .collect::<Vec<_>>()
            .join(", ");

        if operands_str.is_empty() {
            format!("{:08X}  {}", self.address, self.mnemonic)
        } else {
            format!("{:08X}  {}  {}", self.address, self.mnemonic, operands_str)
        }
    }

    /// Format bytes as hex string
    pub fn bytes_to_hex(&self) -> String {
        self.bytes
            .iter()
            .map(|b| format!("{:02X}", b))
            .collect::<Vec<_>>()
            .join(" ")
    }
}

/// Opcode information entry
#[derive(Clone, Copy)]
struct OpcodeInfo {
    mnemonic: &'static str,
    format: &'static str,
    category: OpcodeCategory,
    stack_delta: i32,
    is_branch: bool,
    is_conditional_branch: bool,
    is_call: bool,
    is_return: bool,
}

impl OpcodeInfo {
    const fn new(
        mnemonic: &'static str,
        format: &'static str,
        category: OpcodeCategory,
        stack_delta: i32,
    ) -> Self {
        Self {
            mnemonic,
            format,
            category,
            stack_delta,
            is_branch: false,
            is_conditional_branch: false,
            is_call: false,
            is_return: false,
        }
    }

    const fn with_branch(mut self, conditional: bool) -> Self {
        self.is_branch = true;
        self.is_conditional_branch = conditional;
        self
    }

    const fn with_call(mut self) -> Self {
        self.is_call = true;
        self
    }

    const fn with_return(mut self) -> Self {
        self.is_return = true;
        self
    }
}

/// Get opcode information for standard opcodes (0x00-0xFA)
fn get_opcode_info(opcode: u8) -> &'static OpcodeInfo {
    // Define only the most common/important opcodes
    // This is a subset - expand as needed
    static OPCODES: [OpcodeInfo; 256] = {
        let mut table = [OpcodeInfo::new("Unknown", "", OpcodeCategory::Unknown, 0); 256];

        // Control flow
        table[0x13] =
            OpcodeInfo::new("ExitProcHresult", "", OpcodeCategory::ControlFlow, 0).with_return();
        table[0x14] = OpcodeInfo::new("ExitProc", "", OpcodeCategory::ControlFlow, 0).with_return();
        table[0x1C] =
            OpcodeInfo::new("BranchF", "l", OpcodeCategory::ControlFlow, -1).with_branch(true);
        table[0x1D] =
            OpcodeInfo::new("BranchT", "l", OpcodeCategory::ControlFlow, -1).with_branch(true);
        table[0x1E] =
            OpcodeInfo::new("Branch", "l", OpcodeCategory::ControlFlow, 0).with_branch(false);
        table[0x4B] = OpcodeInfo::new("OnErrorGoto", "l", OpcodeCategory::ControlFlow, 0);

        // Stack operations - literals
        table[0x1B] = OpcodeInfo::new("LitStr", "z", OpcodeCategory::Stack, 1);
        table[0x27] = OpcodeInfo::new("LitVar_Missing", "", OpcodeCategory::Stack, 1);
        table[0x28] = OpcodeInfo::new("LitVarI2", "a%", OpcodeCategory::Stack, 1);
        table[0x3A] = OpcodeInfo::new("LitVarStr", "az", OpcodeCategory::Stack, 1);
        table[0x5E] = OpcodeInfo::new("LitI2", "a%", OpcodeCategory::Stack, 1);
        table[0x5F] = OpcodeInfo::new("LitI4", "d&", OpcodeCategory::Stack, 1);
        table[0x60] = OpcodeInfo::new("LitR4", "f!", OpcodeCategory::Stack, 1);
        table[0x61] = OpcodeInfo::new("LitR8", "g#", OpcodeCategory::Stack, 1);
        table[0xA7] = OpcodeInfo::new("LitVarI2_Byte", "b%", OpcodeCategory::Stack, 1);

        // Variable operations
        table[0x04] = OpcodeInfo::new("FLdRfVar", "a", OpcodeCategory::Variable, 1);
        table[0x43] = OpcodeInfo::new("FStStrCopy", "a", OpcodeCategory::String, -1);
        table[0x62] = OpcodeInfo::new("FLdPrThis", "", OpcodeCategory::Variable, 1);
        table[0x69] = OpcodeInfo::new("FLdI2", "a", OpcodeCategory::Variable, 1);
        table[0x6A] = OpcodeInfo::new("FLdI4", "a", OpcodeCategory::Variable, 1);
        table[0x6D] = OpcodeInfo::new("FStI2", "a", OpcodeCategory::Variable, -1);
        table[0x6E] = OpcodeInfo::new("FStI4", "a", OpcodeCategory::Variable, -1);

        // Function/method calls
        table[0x05] = OpcodeInfo::new("ImpAdLdRf", "c", OpcodeCategory::Call, 1);
        table[0x09] = OpcodeInfo::new("ImpAdCallHresult", "", OpcodeCategory::Call, 0).with_call();
        table[0x0A] = OpcodeInfo::new("ImpAdCallFPR4", "x", OpcodeCategory::Call, 0).with_call();
        table[0x0D] = OpcodeInfo::new("VCallHresult", "v", OpcodeCategory::Call, 0).with_call();
        table[0x7F] = OpcodeInfo::new("CallHresult", "n", OpcodeCategory::Call, 0).with_call();
        table[0x80] = OpcodeInfo::new("CallI2", "n", OpcodeCategory::Call, 1).with_call();
        table[0x81] = OpcodeInfo::new("CallI4", "n", OpcodeCategory::Call, 1).with_call();

        // String operations
        table[0x2A] = OpcodeInfo::new("ConcatStr", "", OpcodeCategory::String, -1);
        table[0x2F] = OpcodeInfo::new("FFree1Str", "", OpcodeCategory::String, 0);
        table[0x32] = OpcodeInfo::new("FFreeStr", "", OpcodeCategory::String, 0);
        table[0x33] = OpcodeInfo::new("LdFixedStr", "z", OpcodeCategory::String, 1);
        table[0x34] = OpcodeInfo::new("CStr2Ansi", "", OpcodeCategory::String, 0);
        table[0x4A] = OpcodeInfo::new("FnLenStr", "", OpcodeCategory::String, 0);

        // Array operations
        table[0x3B] = OpcodeInfo::new("Ary1StStrCopy", "", OpcodeCategory::Array, -2);
        table[0x40] = OpcodeInfo::new("Ary1LdRf", "", OpcodeCategory::Array, 0);
        table[0x41] = OpcodeInfo::new("Ary1LdPr", "", OpcodeCategory::Array, 0);

        // Memory management
        table[0x1A] = OpcodeInfo::new("FFree1Ad", "", OpcodeCategory::Memory, 0);
        table[0x29] = OpcodeInfo::new("FFreeAd", "", OpcodeCategory::Memory, 0);
        table[0x35] = OpcodeInfo::new("FFree1Var", "", OpcodeCategory::Memory, 0);
        table[0x36] = OpcodeInfo::new("FFreeVar", "", OpcodeCategory::Memory, 0);

        // Arithmetic
        table[0x95] = OpcodeInfo::new("AddI2", "", OpcodeCategory::Arithmetic, -1);
        table[0x96] = OpcodeInfo::new("SubI2", "", OpcodeCategory::Arithmetic, -1);
        table[0x97] = OpcodeInfo::new("MulI2", "", OpcodeCategory::Arithmetic, -1);
        table[0x9A] = OpcodeInfo::new("NegI2", "", OpcodeCategory::Arithmetic, 0);

        // Comparison
        table[0xA0] = OpcodeInfo::new("EqI2", "", OpcodeCategory::Comparison, -1);
        table[0xA1] = OpcodeInfo::new("NeI2", "", OpcodeCategory::Comparison, -1);
        table[0xA2] = OpcodeInfo::new("LeI2", "", OpcodeCategory::Comparison, -1);
        table[0xA3] = OpcodeInfo::new("GeI2", "", OpcodeCategory::Comparison, -1);
        table[0xA4] = OpcodeInfo::new("LtI2", "", OpcodeCategory::Comparison, -1);
        table[0xA5] = OpcodeInfo::new("GtI2", "", OpcodeCategory::Comparison, -1);

        table
    };

    &OPCODES[opcode as usize]
}

/// Check if opcode is extended (0xFB-0xFF)
fn is_extended_opcode(opcode: u8) -> bool {
    opcode >= 0xFB
}

/// P-Code disassembler
pub struct Disassembler {
    data: Vec<u8>,
    offset: usize,
}

impl Disassembler {
    /// Create a new disassembler for the given P-Code bytes
    pub fn new(data: Vec<u8>) -> Self {
        Self { data, offset: 0 }
    }

    /// Disassemble all instructions starting from the current offset
    pub fn disassemble(&mut self, address: u32) -> Result<Vec<Instruction>> {
        let mut instructions = Vec::new();
        let mut current_address = address;

        while self.offset < self.data.len() {
            match self.disassemble_one(current_address) {
                Ok(instr) => {
                    current_address += instr.bytes.len() as u32;

                    // Check if this is a return instruction
                    let is_return = instr.is_return;

                    instructions.push(instr);

                    // Stop at procedure exit
                    if is_return {
                        break;
                    }
                }
                Err(e) => {
                    // If we encounter an error, stop disassembly
                    eprintln!("Disassembly error at offset {}: {}", self.offset, e);
                    break;
                }
            }
        }

        Ok(instructions)
    }

    /// Disassemble a single instruction at the current offset
    fn disassemble_one(&mut self, address: u32) -> Result<Instruction> {
        let start_offset = self.offset;

        if self.offset >= self.data.len() {
            return Err(Error::parse("Unexpected end of P-Code"));
        }

        // Read primary opcode
        let opcode = self.read_byte()?;
        let mut instr = Instruction::new(address, opcode);

        // Check for extended opcode
        if is_extended_opcode(opcode) {
            let ext_opcode = self.read_byte()?;
            instr.extended_opcode = Some(ext_opcode);
            instr.mnemonic = format!("Extended_{:02X}_{:02X}", opcode, ext_opcode);
            instr.category = OpcodeCategory::Unknown;
        } else {
            // Standard opcode
            let opcode_info = get_opcode_info(opcode);
            instr.mnemonic = opcode_info.mnemonic.to_string();
            instr.category = opcode_info.category;
            instr.stack_delta = opcode_info.stack_delta;
            instr.is_branch = opcode_info.is_branch;
            instr.is_conditional_branch = opcode_info.is_conditional_branch;
            instr.is_call = opcode_info.is_call;
            instr.is_return = opcode_info.is_return;

            // Decode operands based on format string
            self.decode_operands(&mut instr, opcode_info.format)?;
        }

        // Copy raw bytes
        instr.bytes = self.data[start_offset..self.offset].to_vec();

        Ok(instr)
    }

    /// Decode operands based on format string
    fn decode_operands(&mut self, instr: &mut Instruction, format: &str) -> Result<()> {
        for ch in format.bytes() {
            match ch {
                b'a' => {
                    // Byte argument
                    let val = self.read_byte()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Byte(val), PCodeType::Unknown));
                }
                b'b' => {
                    // Byte literal
                    let val = self.read_byte()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Byte(val), PCodeType::Byte));
                }
                b'c' => {
                    // Control reference (2 bytes)
                    let val = self.read_i16()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Int16(val), PCodeType::Unknown));
                }
                b'd' => {
                    // 32-bit integer literal
                    let val = self.read_i32()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Int32(val), PCodeType::Long));
                }
                b'f' => {
                    // 32-bit float literal
                    let val = self.read_f32()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Float(val), PCodeType::Single));
                }
                b'l' => {
                    // Branch offset (2 bytes, signed)
                    let offset = self.read_i16()?;
                    instr.branch_offset = Some(offset as i32);
                    instr.operands.push(Operand::new(
                        OperandValue::Int16(offset),
                        PCodeType::Unknown,
                    ));
                }
                b'n' => {
                    // Call argument count (2 bytes)
                    let val = self.read_i16()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Int16(val), PCodeType::Unknown));
                }
                b'v' => {
                    // VTable entry (2 bytes)
                    let val = self.read_i16()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Int16(val), PCodeType::Unknown));
                }
                b'x' => {
                    // Extended argument
                    let val = self.read_byte()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::Byte(val), PCodeType::Unknown));
                }
                b'z' => {
                    // Null-terminated string
                    let s = self.read_string()?;
                    instr
                        .operands
                        .push(Operand::new(OperandValue::String(s), PCodeType::String));
                }
                b'%' | b'&' | b'!' | b'#' | b'~' => {
                    // Type suffix - already captured in previous operand
                }
                _ => {
                    // Unknown format character - skip
                }
            }
        }

        Ok(())
    }

    /// Read a single byte
    fn read_byte(&mut self) -> Result<u8> {
        if self.offset >= self.data.len() {
            return Err(Error::parse("Unexpected end of data"));
        }
        let val = self.data[self.offset];
        self.offset += 1;
        Ok(val)
    }

    /// Read a 16-bit signed integer (little-endian)
    fn read_i16(&mut self) -> Result<i16> {
        if self.offset + 2 > self.data.len() {
            return Err(Error::parse("Unexpected end of data"));
        }
        let val = i16::from_le_bytes([self.data[self.offset], self.data[self.offset + 1]]);
        self.offset += 2;
        Ok(val)
    }

    /// Read a 32-bit signed integer (little-endian)
    fn read_i32(&mut self) -> Result<i32> {
        if self.offset + 4 > self.data.len() {
            return Err(Error::parse("Unexpected end of data"));
        }
        let val = i32::from_le_bytes([
            self.data[self.offset],
            self.data[self.offset + 1],
            self.data[self.offset + 2],
            self.data[self.offset + 3],
        ]);
        self.offset += 4;
        Ok(val)
    }

    /// Read a 32-bit float (little-endian)
    fn read_f32(&mut self) -> Result<f32> {
        if self.offset + 4 > self.data.len() {
            return Err(Error::parse("Unexpected end of data"));
        }
        let val = f32::from_le_bytes([
            self.data[self.offset],
            self.data[self.offset + 1],
            self.data[self.offset + 2],
            self.data[self.offset + 3],
        ]);
        self.offset += 4;
        Ok(val)
    }

    /// Read a null-terminated string
    fn read_string(&mut self) -> Result<String> {
        let start = self.offset;
        while self.offset < self.data.len() && self.data[self.offset] != 0 {
            self.offset += 1;
        }

        if self.offset >= self.data.len() {
            return Err(Error::parse("Unterminated string"));
        }

        let s = String::from_utf8_lossy(&self.data[start..self.offset]).to_string();
        self.offset += 1; // Skip null terminator
        Ok(s)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_exit_proc_opcode() {
        let data = vec![0x14]; // ExitProc
        let mut disasm = Disassembler::new(data);
        let result = disasm.disassemble(0x1000).unwrap();

        assert_eq!(result.len(), 1);
        assert_eq!(result[0].mnemonic, "ExitProc");
        assert!(result[0].is_return);
    }

    #[test]
    fn test_branch_opcode() {
        let data = vec![0x1E, 0x10, 0x00]; // Branch +16
        let mut disasm = Disassembler::new(data);
        let result = disasm.disassemble(0x1000).unwrap();

        assert_eq!(result.len(), 1);
        assert_eq!(result[0].mnemonic, "Branch");
        assert!(result[0].is_branch);
        assert_eq!(result[0].branch_offset, Some(16));
    }

    #[test]
    fn test_lit_i2_opcode() {
        let data = vec![0x5E, 0x2A, 0x14]; // LitI2 42, ExitProc (removed extra byte)
        let mut disasm = Disassembler::new(data);
        let result = disasm.disassemble(0x1000).unwrap();

        assert_eq!(result.len(), 2);
        assert_eq!(result[0].mnemonic, "LitI2");
        assert_eq!(result[0].operands.len(), 1);
    }
}
