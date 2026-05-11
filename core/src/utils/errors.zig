// Error types and utilities
const std = @import("std");

pub const VBDecompError = error{
    // PE errors
    InvalidDOSSignature,
    InvalidPESignature,
    InvalidMachineType,
    InvalidOptionalHeader,
    SectionNotFound,
    InvalidRVA,
    OutOfBounds,
    NotSupported,
    
    // VB6 errors
    NotVBBinary,
    UnsupportedVersion,
    CorruptedStructure,
    
    // General errors
    FileNotFound,
    AccessDenied,
    OutOfMemory,
    InvalidParameter,
    NotImplemented,
};

pub fn formatError(err: anyerror, writer: anytype) !void {
    const msg = switch (err) {
        error.InvalidDOSSignature => "Invalid DOS signature (not a valid PE file)",
        error.InvalidPESignature => "Invalid PE signature",
        error.InvalidMachineType => "Unsupported machine type (VB6 requires x86)",
        error.InvalidOptionalHeader => "Invalid or corrupt optional header",
        error.SectionNotFound => "Required section not found",
        error.InvalidRVA => "Invalid relative virtual address",
        error.OutOfBounds => "Address out of bounds",
        error.NotSupported => "Feature not supported",
        error.NotVBBinary => "This is not a Visual Basic binary",
        error.UnsupportedVersion => "Unsupported VB version",
        error.CorruptedStructure => "Corrupted or invalid structure",
        error.FileNotFound => "File not found",
        error.AccessDenied => "Access denied",
        error.OutOfMemory => "Out of memory",
        error.InvalidParameter => "Invalid parameter",
        error.NotImplemented => "Feature not yet implemented",
        else => "Unknown error",
    };
    
    try writer.print("{s}", .{msg});
}
