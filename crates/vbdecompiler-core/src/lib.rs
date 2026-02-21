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

pub mod codegen;
pub mod decompiler;
pub mod error;
pub mod ir;
pub mod lifter;
pub mod pcode;
pub mod pe;
pub mod vb;

pub use decompiler::{DecompilationResult, Decompiler};
pub use error::{Error, Result};
