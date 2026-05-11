// VB Decompiler CLI tool
const std = @import("std");

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    
    _ = allocator; // Mark as used for now
    
    // Print a simple message for now - full CLI will be implemented later
    const c = @cImport({
        @cInclude("stdio.h");
    });
    
    _ = c.printf("VBDecompiler CLI v0.1.0\n");
    _ = c.printf("Usage: vbdecomp <command> <file>\n");
    _ = c.printf("\nCommands:\n");
    _ = c.printf("  analyze <file>   - Analyze VB6 binary\n");
    _ = c.printf("  sections <file>  - List PE sections\n");
    _ = c.printf("\nNote: Full CLI implementation coming soon.\n");
    _ = c.printf("Use the library API (libvbdecomp) for programmatic access.\n");
}
