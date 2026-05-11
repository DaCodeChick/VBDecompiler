/**
 * VB Decompiler C API
 * 
 * This header provides a C-compatible API for the VB Decompiler library.
 * It can be used from C, C++, or any language with C FFI support.
 * 
 * License: LGPL v3
 */

#ifndef VBDECOMP_H
#define VBDECOMP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define VBDECOMP_VERSION_MAJOR 0
#define VBDECOMP_VERSION_MINOR 1
#define VBDECOMP_VERSION_PATCH 0

/* Opaque context handle */
typedef struct vbdecomp_context vbdecomp_context_t;

/* Binary types */
typedef enum {
    VBDECOMP_BINARY_UNKNOWN = 0,
    VBDECOMP_BINARY_EXE = 1,
    VBDECOMP_BINARY_DLL = 2,
    VBDECOMP_BINARY_OCX = 3,
} vbdecomp_binary_type_t;

/* Compilation types */
typedef enum {
    VBDECOMP_COMPILE_UNKNOWN = 0,
    VBDECOMP_COMPILE_NATIVE = 1,
    VBDECOMP_COMPILE_PCODE = 2,
} vbdecomp_compilation_type_t;

/* VB version */
typedef enum {
    VBDECOMP_VB_UNKNOWN = 0,
    VBDECOMP_VB_5 = 5,
    VBDECOMP_VB_6 = 6,
} vbdecomp_vb_version_t;

/* Error codes */
typedef enum {
    VBDECOMP_OK = 0,
    VBDECOMP_ERROR_FILE_NOT_FOUND = -1,
    VBDECOMP_ERROR_INVALID_PE = -2,
    VBDECOMP_ERROR_NOT_VB = -3,
    VBDECOMP_ERROR_INVALID_CONTEXT = -4,
    VBDECOMP_ERROR_OUT_OF_MEMORY = -5,
    VBDECOMP_ERROR_INVALID_ADDRESS = -6,
    VBDECOMP_ERROR_NOT_IMPLEMENTED = -99,
} vbdecomp_error_t;

/* Binary information */
typedef struct {
    bool is_vb;
    vbdecomp_vb_version_t vb_version;
    vbdecomp_binary_type_t binary_type;
    vbdecomp_compilation_type_t compilation_type;
    bool has_forms;
    const char* runtime_dll;
    uint32_t entry_point;
    uint32_t image_base;
    size_t image_size;
} vbdecomp_info_t;

/* Function information */
typedef struct {
    uint32_t address;
    uint32_t size;
    const char* name;
    bool is_export;
    bool is_thunk;
} vbdecomp_function_t;

/* Section information */
typedef struct {
    char name[9]; // 8 chars + null terminator
    uint32_t virtual_address;
    uint32_t virtual_size;
    uint32_t raw_size;
    uint32_t characteristics;
} vbdecomp_section_t;

/* Import information */
typedef struct {
    const char* dll_name;
    const char* function_name;
    uint16_t ordinal;
    uint32_t address;
} vbdecomp_import_t;

/* Export information */
typedef struct {
    const char* name;
    uint16_t ordinal;
    uint32_t address;
} vbdecomp_export_t;

/* String information */
typedef struct {
    uint32_t address;
    const char* value;
    size_t length;
} vbdecomp_string_t;

/*
 * Core API Functions
 */

/* Initialize library (optional, for global setup) */
void vbdecomp_init(void);

/* Cleanup library (optional, for global cleanup) */
void vbdecomp_cleanup(void);

/* Get library version string */
const char* vbdecomp_version(void);

/* Open a VB binary file for analysis */
vbdecomp_context_t* vbdecomp_open(const char* path);

/* Open from memory buffer */
vbdecomp_context_t* vbdecomp_open_memory(const uint8_t* data, size_t size);

/* Close context and free resources */
void vbdecomp_close(vbdecomp_context_t* ctx);

/* Get last error message */
const char* vbdecomp_get_error(vbdecomp_context_t* ctx);

/*
 * Information API
 */

/* Get binary information */
bool vbdecomp_get_info(vbdecomp_context_t* ctx, vbdecomp_info_t* info);

/* Get number of sections */
size_t vbdecomp_get_section_count(vbdecomp_context_t* ctx);

/* Get section by index */
bool vbdecomp_get_section(vbdecomp_context_t* ctx, size_t index, vbdecomp_section_t* section);

/* Get number of imports */
size_t vbdecomp_get_import_count(vbdecomp_context_t* ctx);

/* Get import by index */
bool vbdecomp_get_import(vbdecomp_context_t* ctx, size_t index, vbdecomp_import_t* import);

/* Get number of exports */
size_t vbdecomp_get_export_count(vbdecomp_context_t* ctx);

/* Get export by index */
bool vbdecomp_get_export(vbdecomp_context_t* ctx, size_t index, vbdecomp_export_t* exp);

/* Get number of strings */
size_t vbdecomp_get_string_count(vbdecomp_context_t* ctx);

/* Get string by index */
bool vbdecomp_get_string(vbdecomp_context_t* ctx, size_t index, vbdecomp_string_t* str);

/*
 * Analysis API (placeholder for future implementation)
 */

/* Get number of functions */
size_t vbdecomp_get_function_count(vbdecomp_context_t* ctx);

/* Get function by index */
bool vbdecomp_get_function(vbdecomp_context_t* ctx, size_t index, vbdecomp_function_t* func);

/* Disassemble at address (returns allocated string, caller must free) */
char* vbdecomp_disassemble(vbdecomp_context_t* ctx, uint32_t address, size_t count);

/* Analyze function and build CFG starting at address */
bool vbdecomp_analyze_function(vbdecomp_context_t* ctx, uint32_t address);

/* Get cross-references to an address (returns count, fills buffer if provided) */
size_t vbdecomp_get_xrefs_to(vbdecomp_context_t* ctx, uint32_t address, uint32_t* buffer, size_t buffer_size);

/* Get cross-references from an address (returns count, fills buffer if provided) */
size_t vbdecomp_get_xrefs_from(vbdecomp_context_t* ctx, uint32_t address, uint32_t* buffer, size_t buffer_size);

/* Decompile function at address (returns allocated string, caller must free) */
char* vbdecomp_decompile(vbdecomp_context_t* ctx, uint32_t address);

/* Free string allocated by library */
void vbdecomp_free_string(char* str);

/*
 * Memory API
 */

/* Read bytes from RVA */
size_t vbdecomp_read_bytes(vbdecomp_context_t* ctx, uint32_t rva, uint8_t* buffer, size_t size);

/* Convert RVA to file offset */
bool vbdecomp_rva_to_offset(vbdecomp_context_t* ctx, uint32_t rva, uint32_t* offset);

#ifdef __cplusplus
}
#endif

#endif /* VBDECOMP_H */
