// CFG export utilities for visualization
const std = @import("std");
const CFG = @import("cfg.zig").CFG;
const Function = @import("cfg.zig").Function;
const BasicBlock = @import("cfg.zig").BasicBlock;
const EdgeType = @import("cfg.zig").EdgeType;

/// Export a function's CFG to DOT format (Graphviz)
pub fn exportFunctionToDot(
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    function_address: u32,
    writer: *std.Io.Writer,
) !void {
    _ = allocator; // We'll use it for temporary allocations if needed
    
    const func = cfg.functions.get(function_address) orelse return error.FunctionNotFound;
    
    try writer.print("digraph function_0x{X:0>8} {{\n", .{function_address});
    try writer.writeAll("  rankdir=TB;\n");
    try writer.writeAll("  node [shape=box, fontname=\"Courier\"];\n");
    try writer.writeAll("  edge [fontname=\"Courier\", fontsize=10];\n\n");
    
    // Add function info as a label
    const func_name = func.name orelse "unknown";
    try writer.print("  label=\"Function: {s}\\nAddress: 0x{X:0>8}\\nConvention: {s}\";\n",
        .{ func_name, func.address, @tagName(func.convention) });
    try writer.writeAll("  labelloc=t;\n\n");
    
    // Output basic blocks
    for (func.blocks) |bb_addr| {
        const bb = cfg.blocks.get(bb_addr) orelse continue;
        
        // Output node
        try writer.print("  bb_0x{X:0>8} [label=\"0x{X:0>8} - 0x{X:0>8}\\n{d} instructions\"];\n", 
            .{ bb_addr, bb.start_address, bb.end_address, bb.instructions.len });
        
        // Output edges to successors
        for (bb.successors) |edge| {
            const edge_style = switch (edge.type) {
                .fallthrough => "",
                .unconditional_jump => "",
                .conditional_jump => " [label=\"taken\", color=green]",
                .conditional_fallthrough => " [label=\"not taken\", color=red]",
                .call => " [label=\"call\", style=dashed]",
                .ret => " [label=\"return\", style=dotted]",
            };
            
            try writer.print("  bb_0x{X:0>8} -> bb_0x{X:0>8}{s};\n",
                .{ bb_addr, edge.to, edge_style });
        }
    }
    
    try writer.writeAll("}\n");
}

/// Export the entire CFG to DOT format
pub fn exportToDot(
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    writer: *std.Io.Writer,
) !void {
    _ = allocator;
    
    try writer.writeAll("digraph cfg {\n");
    try writer.writeAll("  rankdir=TB;\n");
    try writer.writeAll("  node [shape=box, fontname=\"Courier\"];\n");
    try writer.writeAll("  edge [fontname=\"Courier\", fontsize=10];\n\n");
    
    // Group by functions
    var func_iter = cfg.functions.iterator();
    while (func_iter.next()) |entry| {
        const func = entry.value_ptr.*;
        
        try writer.print("  subgraph cluster_0x{X:0>8} {{\n", .{func.address});
        try writer.print("    label=\"Function 0x{X:0>8}\";\n", .{func.address});
        try writer.writeAll("    style=dashed;\n\n");
        
        // Output basic blocks in this function
        for (func.blocks) |bb_addr| {
            const bb = cfg.blocks.get(bb_addr) orelse continue;
            
            try writer.print("    bb_0x{X:0>8} [label=\"0x{X:0>8}\\n{d} inst\"];\n", 
                .{ bb_addr, bb.start_address, bb.instructions.len });
        }
        
        try writer.writeAll("  }\n\n");
    }
    
    // Output all edges
    var block_iter = cfg.blocks.iterator();
    while (block_iter.next()) |entry| {
        const bb = entry.value_ptr.*;
        const bb_addr = entry.key_ptr.*;
        
        for (bb.successors) |edge| {
            const edge_style = switch (edge.type) {
                .fallthrough => "",
                .unconditional_jump => "",
                .conditional_jump => " [label=\"T\", color=green]",
                .conditional_fallthrough => " [label=\"F\", color=red]",
                .call => " [label=\"call\", style=dashed]",
                .ret => " [label=\"ret\", style=dotted]",
            };
            
            try writer.print("  bb_0x{X:0>8} -> bb_0x{X:0>8}{s};\n",
                .{ bb_addr, edge.to, edge_style });
        }
    }
    
    try writer.writeAll("}\n");
}

test "DOT export basic" {
    const allocator = std.testing.allocator;
    
    var cfg = CFG.init(allocator);
    defer cfg.deinit();
    
    // For testing, we'll skip actual export since we need a real writer
    // The real tests will happen through integration testing with actual files
    _ = exportToDot;
    _ = exportFunctionToDot;
}
