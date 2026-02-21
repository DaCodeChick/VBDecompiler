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
use vbdecompiler_core::Decompiler;

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
