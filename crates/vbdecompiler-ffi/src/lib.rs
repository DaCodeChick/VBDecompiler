// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! C FFI bindings for VBDecompiler
//!
//! This crate provides a C-compatible interface to the Rust core library,
//! allowing the C++/Qt GUI to call into the Rust decompiler.

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use std::ptr;
use vbdecompiler_core::{Decompiler, X86Disassembler};

/// Opaque handle to a Decompiler instance
#[repr(C)]
pub struct VBDecompilerHandle {
    _private: [u8; 0],
}

/// Result structure for C FFI
#[repr(C)]
pub struct VBDecompilationResult {
    /// Project name (must be freed with vbdecompiler_free_string)
    pub project_name: *mut c_char,
    /// VB6 code (must be freed with vbdecompiler_free_string)
    pub vb6_code: *mut c_char,
    /// Whether P-Code or native
    pub is_pcode: bool,
    /// Number of objects
    pub object_count: usize,
    /// Number of methods
    pub method_count: usize,
}

/// Create a new decompiler instance
#[no_mangle]
pub extern "C" fn vbdecompiler_new() -> *mut VBDecompilerHandle {
    let decompiler = Box::new(Decompiler::new());
    Box::into_raw(decompiler) as *mut VBDecompilerHandle
}

/// Free a decompiler instance
#[no_mangle]
pub extern "C" fn vbdecompiler_free(handle: *mut VBDecompilerHandle) {
    if !handle.is_null() {
        unsafe {
            let _ = Box::from_raw(handle as *mut Decompiler);
        }
    }
}

/// Decompile a file
///
/// Returns 0 on success, non-zero error code on failure
/// On success, result must be freed with vbdecompiler_free_result
#[no_mangle]
pub extern "C" fn vbdecompiler_decompile_file(
    handle: *mut VBDecompilerHandle,
    path: *const c_char,
    result: *mut *mut VBDecompilationResult,
) -> c_int {
    if handle.is_null() || path.is_null() || result.is_null() {
        return -1; // Invalid argument
    }

    let decompiler = unsafe { &mut *(handle as *mut Decompiler) };

    let path_str = match unsafe { CStr::from_ptr(path) }.to_str() {
        Ok(s) => s,
        Err(_) => return -2, // Invalid UTF-8
    };

    match decompiler.decompile_file(path_str) {
        Ok(res) => {
            let c_result = Box::new(VBDecompilationResult {
                project_name: match CString::new(res.project_name) {
                    Ok(s) => s.into_raw(),
                    Err(_) => ptr::null_mut(),
                },
                vb6_code: match CString::new(res.vb6_code) {
                    Ok(s) => s.into_raw(),
                    Err(_) => ptr::null_mut(),
                },
                is_pcode: res.is_pcode,
                object_count: res.object_count,
                method_count: res.method_count,
            });

            unsafe {
                *result = Box::into_raw(c_result);
            }
            0 // Success
        }
        Err(_) => -3, // Decompilation error
    }
}

/// Free a decompilation result
#[no_mangle]
pub extern "C" fn vbdecompiler_free_result(result: *mut VBDecompilationResult) {
    if !result.is_null() {
        unsafe {
            let res = Box::from_raw(result);
            if !res.project_name.is_null() {
                let _ = CString::from_raw(res.project_name);
            }
            if !res.vb6_code.is_null() {
                let _ = CString::from_raw(res.vb6_code);
            }
        }
    }
}

/// Free a string allocated by the library
#[no_mangle]
pub extern "C" fn vbdecompiler_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s);
        }
    }
}

/// Get last error message (returns NULL if no error)
#[no_mangle]
pub extern "C" fn vbdecompiler_last_error() -> *const c_char {
    // TODO: Implement thread-local error storage
    ptr::null()
}

// ============================================================================
// X86 Disassembler FFI
// ============================================================================

/// Opaque handle to an X86Disassembler instance
#[repr(C)]
pub struct X86DisassemblerHandle {
    _private: [u8; 0],
}

/// X86 instruction result
#[repr(C)]
pub struct X86InstructionResult {
    /// Address of instruction
    pub address: u64,
    /// Instruction text (must be freed with vbdecompiler_free_string)
    pub text: *mut c_char,
    /// Instruction length in bytes
    pub length: usize,
    /// Instruction bytes (up to 15 bytes for x86)
    pub bytes: [u8; 15],
    /// Actual number of bytes in the instruction
    pub bytes_count: usize,
}

/// Create a new x86 disassembler (32-bit mode)
#[no_mangle]
pub extern "C" fn x86_disassembler_new() -> *mut X86DisassemblerHandle {
    let disasm = Box::new(X86Disassembler::new_32bit());
    Box::into_raw(disasm) as *mut X86DisassemblerHandle
}

/// Create a new x86 disassembler with specific bitness
#[no_mangle]
pub extern "C" fn x86_disassembler_new_with_bitness(bitness: u32) -> *mut X86DisassemblerHandle {
    let disasm = Box::new(X86Disassembler::new(bitness));
    Box::into_raw(disasm) as *mut X86DisassemblerHandle
}

/// Free an x86 disassembler instance
#[no_mangle]
pub extern "C" fn x86_disassembler_free(handle: *mut X86DisassemblerHandle) {
    if !handle.is_null() {
        unsafe {
            let _ = Box::from_raw(handle as *mut X86Disassembler);
        }
    }
}

/// Disassemble bytes
///
/// Returns number of instructions disassembled, or -1 on error
/// results array must be freed with x86_disassembler_free_results
#[no_mangle]
pub extern "C" fn x86_disassemble(
    handle: *mut X86DisassemblerHandle,
    code: *const u8,
    code_len: usize,
    address: u64,
    results: *mut *mut X86InstructionResult,
    count: *mut usize,
) -> c_int {
    if handle.is_null() || code.is_null() || results.is_null() || count.is_null() {
        return -1;
    }

    let disasm = unsafe { &*(handle as *const X86Disassembler) };
    let code_slice = unsafe { std::slice::from_raw_parts(code, code_len) };

    match disasm.disassemble(code_slice, address) {
        Ok(instructions) => {
            let mut c_results = Vec::with_capacity(instructions.len());

            for instr in instructions {
                let mut bytes = [0u8; 15];
                let bytes_count = instr.bytes.len().min(15);
                bytes[..bytes_count].copy_from_slice(&instr.bytes[..bytes_count]);

                let text = match CString::new(instr.text) {
                    Ok(s) => s.into_raw(),
                    Err(_) => ptr::null_mut(),
                };

                c_results.push(X86InstructionResult {
                    address: instr.address,
                    text,
                    length: instr.length,
                    bytes,
                    bytes_count,
                });
            }

            let len = c_results.len();
            unsafe {
                *count = len;
                *results = c_results.as_mut_ptr();
            }
            std::mem::forget(c_results);

            len as c_int
        }
        Err(_) => -1,
    }
}

/// Free disassembly results
#[no_mangle]
pub extern "C" fn x86_disassembler_free_results(results: *mut X86InstructionResult, count: usize) {
    if !results.is_null() && count > 0 {
        unsafe {
            let results_vec = Vec::from_raw_parts(results, count, count);
            for result in results_vec {
                if !result.text.is_null() {
                    let _ = CString::from_raw(result.text);
                }
            }
        }
    }
}
