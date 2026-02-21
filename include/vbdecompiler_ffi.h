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

// ============================================================================
// X86 Disassembler FFI
// ============================================================================

/**
 * Opaque handle to an X86Disassembler instance
 */
typedef struct X86DisassemblerHandle X86DisassemblerHandle;

/**
 * X86 instruction result
 */
typedef struct {
    uint64_t address;       // Address of instruction
    char* text;             // Assembly text (must be freed with vbdecompiler_free_string)
    size_t length;          // Instruction length in bytes
    uint8_t bytes[15];      // Instruction bytes (up to 15 for x86)
    size_t bytes_count;     // Actual number of bytes
} X86InstructionResult;

/**
 * Create a new x86 disassembler (32-bit mode)
 * 
 * @return Opaque handle to disassembler, must be freed with x86_disassembler_free
 */
X86DisassemblerHandle* x86_disassembler_new(void);

/**
 * Create a new x86 disassembler with specific bitness
 * 
 * @param bitness 16, 32, or 64 bit mode
 * @return Opaque handle to disassembler, must be freed with x86_disassembler_free
 */
X86DisassemblerHandle* x86_disassembler_new_with_bitness(uint32_t bitness);

/**
 * Free an x86 disassembler instance
 * 
 * @param handle Disassembler handle to free
 */
void x86_disassembler_free(X86DisassemblerHandle* handle);

/**
 * Disassemble x86 code
 * 
 * @param handle Disassembler handle
 * @param code Byte array to disassemble
 * @param code_len Length of code array
 * @param address Starting address (RVA or virtual address)
 * @param results Output pointer for instruction array (must be freed with x86_disassembler_free_results)
 * @param count Output pointer for number of instructions
 * @return Number of instructions on success, -1 on error
 */
int x86_disassemble(
    X86DisassemblerHandle* handle,
    const uint8_t* code,
    size_t code_len,
    uint64_t address,
    X86InstructionResult** results,
    size_t* count
);

/**
 * Free disassembly results
 * 
 * @param results Results array to free
 * @param count Number of results
 */
void x86_disassembler_free_results(X86InstructionResult* results, size_t count);

#ifdef __cplusplus
}
#endif

#endif // VBDECOMPILER_FFI_H
