// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! Main decompiler orchestrator module
//!
//! Wires together all decompilation stages:
//! PE → VB → P-Code → IR → Code Generation

use crate::codegen::VB6CodeGenerator;
use crate::error::{Error, Result};
use crate::ir::Function;
use crate::lifter::PCodeLifter;
use crate::pcode::Disassembler;
use crate::pe::PEFile;
use crate::vb;
use rayon::prelude::*;
use std::fs;
use std::sync::Arc;

/// Main decompiler orchestrator
pub struct Decompiler {
    generator: VB6CodeGenerator,
}

impl Decompiler {
    pub fn new() -> Self {
        Self {
            generator: VB6CodeGenerator::new(),
        }
    }

    /// Decompile a VB executable file
    pub fn decompile_file(&mut self, path: &str) -> Result<DecompilationResult> {
        log::info!("Decompiling file: {}", path);

        // 1. Read file
        let data = fs::read(path).map_err(|e| Error::Io(e))?;

        // 2. Parse PE file
        log::info!("Parsing PE file...");
        let pe = PEFile::from_bytes(data)?;

        // 3. Parse VB structures
        log::info!("Parsing VB structures...");
        let vb_file = Arc::new(vb::VBFile::from_pe(pe)?);

        log::info!(
            "Found VB project: {}",
            vb_file.project_name().as_deref().unwrap_or("Unknown")
        );

        // 4. Collect all methods to decompile
        let mut methods_to_decompile = Vec::new();

        for (obj_idx, object) in vb_file.objects().iter().enumerate() {
            log::info!("Processing object: {}", object.name);

            for (method_idx, method_name) in object.method_names.iter().enumerate() {
                methods_to_decompile.push((
                    obj_idx,
                    method_idx,
                    object.name.clone(),
                    method_name.clone(),
                ));
            }
        }

        log::info!(
            "Found {} methods, decompiling in parallel with Rayon...",
            methods_to_decompile.len()
        );

        // 5. Decompile methods in parallel using Rayon
        // This provides significant speedup for executables with many methods.
        // Each method is decompiled independently on a separate thread from Rayon's thread pool.
        // Benefits:
        // - Scales with CPU cores (e.g., 8 cores → ~8x faster for 100+ methods)
        // - Memory-safe: Rust's ownership prevents data races
        // - Automatic work stealing: Rayon balances work across threads
        let decompiled_methods: Vec<(String, String)> = methods_to_decompile
            .par_iter()
            .filter_map(|(obj_idx, method_idx, obj_name, method_name)| {
                log::info!("  Processing method: {}_{}", obj_name, method_name);

                // Get P-Code for this specific method
                let pcode_data = match vb_file.get_pcode_for_method(*obj_idx, *method_idx) {
                    Some(data) => data,
                    None => {
                        log::info!("    No P-Code (native compiled)");
                        return None;
                    }
                };

                if pcode_data.is_empty() {
                    log::info!("    Empty P-Code data");
                    return None;
                }

                log::info!(
                    "    P-Code found ({} bytes), disassembling...",
                    pcode_data.len()
                );

                // Disassemble P-Code
                let mut disassembler = Disassembler::new(pcode_data);
                let instructions = match disassembler.disassemble(0) {
                    Ok(insns) => insns,
                    Err(e) => {
                        log::warn!("    Failed to disassemble: {}", e);
                        return None;
                    }
                };

                if instructions.is_empty() {
                    log::warn!("    No instructions found");
                    return None;
                }

                log::info!("    Disassembled {} instructions", instructions.len());

                // Lift P-Code to IR
                let mut lifter = PCodeLifter::new();
                let function_name = format!("{}_{}", obj_name, method_name);
                let function = match lifter.lift(&instructions, function_name.clone(), 0) {
                    Ok(func) => func,
                    Err(e) => {
                        log::warn!("    Failed to lift: {}", e);
                        return None;
                    }
                };

                log::info!("    Lifted to IR: {} blocks", function.basic_blocks.len());

                // Generate VB6 code (each thread gets its own generator)
                let mut generator = VB6CodeGenerator::new();
                let code = generator.generate_function(&function);

                log::info!("    Successfully decompiled {}", function_name);

                Some((function_name, code))
            })
            .collect();

        if decompiled_methods.is_empty() {
            return Err(Error::Decompilation(
                "No P-Code methods found (executable may be native-compiled)".to_string(),
            ));
        }

        // 6. Combine all decompiled code
        let mut vb6_code = String::new();
        for (_name, code) in &decompiled_methods {
            vb6_code.push_str(code);
            vb6_code.push_str("\n\n");
        }

        Ok(DecompilationResult {
            project_name: vb_file
                .project_name()
                .unwrap_or_else(|| "Unknown".to_string()),
            vb6_code,
            is_pcode: true,
            object_count: vb_file.objects().len(),
            method_count: decompiled_methods.len(),
        })
    }

    /// Generate VB6 code from an IR function (for testing/API use)
    pub fn generate_code(&mut self, function: &Function) -> String {
        self.generator.generate_function(function)
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ir::{Expression, Statement, Type, TypeKind, Variable};

    #[test]
    fn test_decompiler_creation() {
        let _decompiler = Decompiler::new();
        // Just test that it creates successfully
    }

    #[test]
    fn test_generate_simple_function() {
        let mut decompiler = Decompiler::new();

        let mut function = Function::new("TestFunc".to_string(), Type::new(TypeKind::Integer));

        // Add a local variable
        let var = Variable::new(0, "x".to_string(), TypeKind::Integer);
        function.add_local_variable(var.clone());

        // Add a simple statement
        let mut block = crate::ir::BasicBlock::new(0);
        block.add_statement(Statement::assign(var, Expression::int_const(42)));
        block.add_statement(Statement::return_stmt(Some(Expression::int_const(42))));
        function.add_basic_block(block);

        let code = decompiler.generate_code(&function);

        assert!(code.contains("Function TestFunc"));
        assert!(code.contains("Dim x As Integer"));
        assert!(code.contains("x = 42"));
        assert!(code.contains("End Function"));
    }
}
