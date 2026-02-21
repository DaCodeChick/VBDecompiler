// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! x86/x64 disassembler module using iced-x86
//!
//! Provides x86 disassembly for native-compiled VB executables

use crate::error::{Error, Result};
use iced_x86::{Decoder, DecoderOptions, Formatter, IntelFormatter};

/// x86 instruction representation
#[derive(Debug, Clone)]
pub struct X86Instruction {
    /// Address of instruction
    pub address: u64,
    /// Instruction bytes
    pub bytes: Vec<u8>,
    /// Assembly mnemonic and operands
    pub text: String,
    /// Instruction length in bytes
    pub length: usize,
}

/// x86 Disassembler using iced-x86
pub struct X86Disassembler {
    bitness: u32,
}

impl X86Disassembler {
    /// Create a new x86 disassembler
    ///
    /// # Arguments
    /// * `bitness` - 16, 32, or 64 bit mode (VB is typically 32-bit)
    pub fn new(bitness: u32) -> Self {
        Self { bitness }
    }

    /// Create a 32-bit disassembler (default for VB executables)
    pub fn new_32bit() -> Self {
        Self::new(32)
    }

    /// Disassemble bytes at given address
    ///
    /// # Arguments
    /// * `code` - Raw bytes to disassemble
    /// * `address` - Starting address (RVA or virtual address)
    ///
    /// # Returns
    /// Vector of disassembled instructions
    pub fn disassemble(&self, code: &[u8], address: u64) -> Result<Vec<X86Instruction>> {
        let mut decoder = Decoder::with_ip(self.bitness, code, address, DecoderOptions::NONE);
        let mut formatter = IntelFormatter::new();
        let mut output = String::new();
        let mut instructions = Vec::new();

        for instr in &mut decoder {
            output.clear();
            formatter.format(&instr, &mut output);

            let len = instr.len();
            let mut bytes = vec![0u8; len];
            bytes.copy_from_slice(&code[(instr.ip() - address) as usize..][..len]);

            instructions.push(X86Instruction {
                address: instr.ip(),
                bytes,
                text: output.clone(),
                length: len,
            });
        }

        Ok(instructions)
    }

    /// Disassemble a single instruction
    pub fn disassemble_one(&self, code: &[u8], address: u64) -> Result<X86Instruction> {
        let mut decoder = Decoder::with_ip(self.bitness, code, address, DecoderOptions::NONE);
        let mut formatter = IntelFormatter::new();
        let mut output = String::new();

        if let Some(instr) = decoder.iter().next() {
            formatter.format(&instr, &mut output);

            let len = instr.len();
            let mut bytes = vec![0u8; len];
            bytes.copy_from_slice(&code[..len]);

            Ok(X86Instruction {
                address: instr.ip(),
                bytes,
                text: output,
                length: len,
            })
        } else {
            Err(Error::Decompilation("No instruction decoded".to_string()))
        }
    }
}

impl Default for X86Disassembler {
    fn default() -> Self {
        Self::new_32bit()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_disassemble_mov_ret() {
        let disasm = X86Disassembler::new_32bit();

        // MOV EAX, 42; RET
        let code = vec![0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3];
        let instructions = disasm.disassemble(&code, 0).unwrap();

        assert_eq!(instructions.len(), 2);

        // First instruction: MOV EAX, 0x2A
        assert_eq!(instructions[0].address, 0);
        assert_eq!(instructions[0].length, 5);
        assert!(instructions[0].text.contains("mov"));
        assert!(instructions[0].text.contains("eax"));

        // Second instruction: RET
        assert_eq!(instructions[1].address, 5);
        assert_eq!(instructions[1].length, 1);
        assert!(instructions[1].text.contains("ret"));
    }

    #[test]
    fn test_disassemble_push_pop() {
        let disasm = X86Disassembler::new_32bit();

        // PUSH EBP; MOV EBP, ESP; POP EBP
        let code = vec![0x55, 0x89, 0xE5, 0x5D];
        let instructions = disasm.disassemble(&code, 0x401000).unwrap();

        assert_eq!(instructions.len(), 3);

        // PUSH EBP
        assert_eq!(instructions[0].address, 0x401000);
        assert!(instructions[0].text.contains("push"));

        // MOV EBP, ESP
        assert_eq!(instructions[1].address, 0x401001);
        assert!(instructions[1].text.contains("mov"));

        // POP EBP
        assert_eq!(instructions[2].address, 0x401003);
        assert!(instructions[2].text.contains("pop"));
    }

    #[test]
    fn test_disassemble_one() {
        let disasm = X86Disassembler::new_32bit();

        // MOV EAX, 42
        let code = vec![0xB8, 0x2A, 0x00, 0x00, 0x00];
        let instr = disasm.disassemble_one(&code, 0).unwrap();

        assert_eq!(instr.address, 0);
        assert_eq!(instr.length, 5);
        assert!(instr.text.contains("mov"));
    }

    #[test]
    fn test_empty_code() {
        let disasm = X86Disassembler::new_32bit();
        let instructions = disasm.disassemble(&[], 0).unwrap();
        assert_eq!(instructions.len(), 0);
    }

    #[test]
    fn test_64bit_mode() {
        let disasm = X86Disassembler::new(64);

        // MOV RAX, 0x123456789ABCDEF0
        let code = vec![0x48, 0xB8, 0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12];
        let instructions = disasm.disassemble(&code, 0).unwrap();

        assert_eq!(instructions.len(), 1);
        assert!(instructions[0].text.contains("mov"));
        assert!(instructions[0].text.contains("rax"));
    }
}
