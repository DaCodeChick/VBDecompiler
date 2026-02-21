// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * C FFI interface to Rust VBDecompiler core
 * 
 * This header allows C++ code to call the Rust decompiler implementation.
 */

#ifndef VBDECOMPILER_FFI_H
#define VBDECOMPILER_FFI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle to a Decompiler instance
 */
typedef struct VBDecompilerHandle VBDecompilerHandle;

/**
 * Decompilation result structure
 */
typedef struct {
    char* project_name;  // Must be freed with vbdecompiler_free_string
    char* vb6_code;      // Must be freed with vbdecompiler_free_string
    bool is_pcode;
    size_t object_count;
    size_t method_count;
} VBDecompilationResult;

/**
 * Create a new decompiler instance
 * 
 * @return Opaque handle to decompiler, must be freed with vbdecompiler_free
 */
VBDecompilerHandle* vbdecompiler_new(void);

/**
 * Free a decompiler instance
 * 
 * @param handle Decompiler handle to free
 */
void vbdecompiler_free(VBDecompilerHandle* handle);

/**
 * Decompile a VB executable file
 * 
 * @param handle Decompiler handle
 * @param path Path to VB executable (.exe, .dll, .ocx)
 * @param result Output pointer for decompilation result (must be freed with vbdecompiler_free_result)
 * @return 0 on success, negative error code on failure
 *         -1: Invalid argument (NULL pointer)
 *         -2: Invalid UTF-8 in path
 *         -3: Decompilation error
 */
int vbdecompiler_decompile_file(
    VBDecompilerHandle* handle,
    const char* path,
    VBDecompilationResult** result
);

/**
 * Free a decompilation result
 * 
 * @param result Result structure to free
 */
void vbdecompiler_free_result(VBDecompilationResult* result);

/**
 * Free a string allocated by the library
 * 
 * @param s String to free
 */
void vbdecompiler_free_string(char* s);

/**
 * Get last error message (returns NULL if no error)
 * 
 * @return Error message string (do not free)
 */
const char* vbdecompiler_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // VBDECOMPILER_FFI_H
