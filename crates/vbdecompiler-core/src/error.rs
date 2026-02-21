// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! Error types for VBDecompiler

/// Result type for VBDecompiler operations
pub type Result<T> = std::result::Result<T, Error>;

/// Error types that can occur during decompilation
#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("Invalid PE file: {0}")]
    InvalidPE(String),

    #[error("Invalid VB structure: {0}")]
    InvalidVB(String),

    #[error("Not a VB file")]
    NotVBFile,

    #[error("P-Code disassembly failed: {0}")]
    PCodeDisassembly(String),

    #[error("IR lift failed: {0}")]
    IRLift(String),

    #[error("Decompilation failed: {0}")]
    Decompilation(String),

    #[error("Not implemented: {0}")]
    NotImplemented(String),

    #[error("Parse error: {0}")]
    Parse(String),

    #[error("Out of bounds access at offset {offset:#x}")]
    OutOfBounds { offset: usize },

    #[error("Unsupported: {0}")]
    Unsupported(String),
}

impl Error {
    /// Create an InvalidPE error
    pub fn invalid_pe(msg: impl Into<String>) -> Self {
        Self::InvalidPE(msg.into())
    }

    /// Create an InvalidVB error
    pub fn invalid_vb(msg: impl Into<String>) -> Self {
        Self::InvalidVB(msg.into())
    }

    /// Create a Parse error
    pub fn parse(msg: impl Into<String>) -> Self {
        Self::Parse(msg.into())
    }

    /// Create an OutOfBounds error
    pub fn out_of_bounds(offset: usize) -> Self {
        Self::OutOfBounds { offset }
    }
}
