// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! Core decompilation engine for Visual Basic 5/6 executables
//!
//! This library provides a complete pipeline for decompiling VB5/6 executables:
//!
//! ```text
//! VB.exe → PE Parser → VB Parser → P-Code Extractor →
//! P-Code Disassembler → IR Lifter → Control Flow Structurer →
//! Type Recovery → VB6 Code Generator
//! ```
//!
//! # Architecture
//!
//! - **pe**: PE file parsing
//! - **vb**: VB structure parsing and P-Code extraction
//! - **pcode**: P-Code disassembler
//! - **ir**: Intermediate representation
//! - **decompiler**: Control flow structuring and code generation
//!
//! # Example
//!
//! ```no_run
//! use vbdecompiler_core::Decompiler;
//!
//! let mut decompiler = Decompiler::new();
//! let result = decompiler.decompile_file("program.exe")?;
//! println!("{}", result.vb6_code);
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

pub mod decompiler;
pub mod error;
pub mod ir;
pub mod lifter;
pub mod pcode;
pub mod pe;
pub mod vb;

pub use error::{Error, Result};

/// Main decompiler interface
pub struct Decompiler {
    // Configuration options can be added here
}

impl Decompiler {
    /// Create a new decompiler instance
    pub fn new() -> Self {
        Self {}
    }

    /// Decompile a VB executable file
    pub fn decompile_file(&mut self, path: &str) -> Result<DecompilationResult> {
        log::info!("Decompiling file: {}", path);

        // TODO: Implement full pipeline
        // 1. Parse PE file
        // 2. Parse VB structures
        // 3. Extract P-Code
        // 4. Disassemble P-Code
        // 5. Lift to IR
        // 6. Structure control flow
        // 7. Generate VB6 code

        Err(Error::NotImplemented("decompile_file".to_string()))
    }
}

impl Default for Decompiler {
    fn default() -> Self {
        Self::new()
    }
}

/// Result of decompilation
#[derive(Debug, Clone)]
pub struct DecompilationResult {
    /// Project name
    pub project_name: String,
    /// Generated VB6 source code
    pub vb6_code: String,
    /// Whether this was P-Code or native
    pub is_pcode: bool,
    /// Number of objects decompiled
    pub object_count: usize,
    /// Number of methods decompiled
    pub method_count: usize,
}
