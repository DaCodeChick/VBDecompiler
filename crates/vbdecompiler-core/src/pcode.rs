// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! P-Code disassembler module

/// P-Code instruction
#[derive(Debug, Clone)]
pub struct PCodeInstruction {
    pub opcode: u8,
    pub mnemonic: String,
    pub operands: Vec<Operand>,
}

/// P-Code operand
#[derive(Debug, Clone)]
pub enum Operand {
    Immediate(i32),
    Register(u8),
    Local(u16),
    Argument(u16),
    String(String),
}
