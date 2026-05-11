const std = @import("std");
const TypeLattice = @import("type_lattice.zig").TypeLattice;
const VBType = @import("type_lattice.zig").VBType;
const TypeInferenceEngine = @import("type_inference.zig").TypeInferenceEngine;
const SSAFunction = @import("../ir/ssa.zig").SSAFunction;
const PCodeBlock = @import("../ir/pcode_function.zig").PCodeBlock;

pub const TypePrinter = struct {
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) TypePrinter {
        return TypePrinter{
            .allocator = allocator,
        };
    }

    pub fn printTypeInference(self: *TypePrinter, writer: anytype, engine: *const TypeInferenceEngine, ssa_func: *const SSAFunction) !void {
        try writer.writeAll("=== Type Inference Results ===\n");
        try writer.print("Function entry: 0x{x:0>8}\n\n", .{ssa_func.entry});

        // Print inferred types for each variable
        try writer.writeAll("Variable Types:\n");
        
        var it = engine.var_types.iterator();
        while (it.next()) |entry| {
            const var_id = entry.key_ptr.*;
            const vb_type = entry.value_ptr.*;
            
            const block_id = @as(u32, @truncate(var_id >> 32));
            const index = @as(u32, @truncate(var_id & 0xFFFFFFFF));
            
            try writer.print("  v{d}_{d}: {s}\n", .{
                block_id,
                index,
                self.typeToString(vb_type),
            });
        }

        try writer.writeAll("\n");
    }

    pub fn printAnnotatedSSA(self: *TypePrinter, writer: anytype, engine: *const TypeInferenceEngine, ssa_func: *const SSAFunction) !void {

        try writer.writeAll("=== Annotated SSA with Types ===\n");
        try writer.print("Function entry: 0x{x:0>8}\n\n", .{ssa_func.entry});

        var block_it = ssa_func.blocks.valueIterator();
        while (block_it.next()) |block| {
            try writer.print("Block 0x{x}:\n", .{block.address});

            for (block.instructions.items, 0..) |inst, idx| {
                const var_id = self.makeVarId(block.address, @as(u32, @intCast(idx)));
                const vb_type = engine.var_types.get(var_id) orelse VBType.top;

                try writer.print("  [{s:8}] ", .{self.typeToString(vb_type)});
                try self.printSSAInstruction(writer, inst);
                try writer.writeAll("\n");
            }

            try writer.writeAll("\n");
        }
    }

    fn printSSAInstruction(self: *TypePrinter, writer: anytype, inst: anytype) !void {
        
        try writer.print("{s}", .{@tagName(inst.opcode)});
        
        if (inst.output) |out| {
            try writer.print(" {s}", .{self.varnodeToString(out)});
        }
        
        if (inst.input0) |in0| {
            try writer.print(", {s}", .{self.varnodeToString(in0)});
        }
        
        if (inst.input1) |in1| {
            try writer.print(", {s}", .{self.varnodeToString(in1)});
        }
    }

    fn varnodeToString(self: *TypePrinter, varnode: anytype) []const u8 {
        _ = self;
        // Simplified varnode printing - in reality, you'd format the varnode properly
        // based on its space (register, ram, constant, etc.)
        var buf: [64]u8 = undefined;
        const str = std.fmt.bufPrint(&buf, "v_{x:0>8}:{d}", .{
            varnode.offset,
            varnode.size,
        }) catch "<?>";
        return str;
    }

    fn typeToString(self: *TypePrinter, vb_type: VBType) []const u8 {
        _ = self;
        return switch (vb_type) {
            .bottom => "⊥",
            .byte => "Byte",
            .boolean => "Boolean",
            .integer => "Integer",
            .long => "Long",
            .single => "Single",
            .double_type => "Double",
            .currency => "Currency",
            .date => "Date",
            .string => "String",
            .object => "Object",
            .variant => "Variant",
            .array => "Array",
            .udt => "UDT",
            .top => "⊤",
        };
    }

    fn makeVarId(self: *TypePrinter, block_id: u32, index: u32) u64 {
        _ = self;
        return (@as(u64, block_id) << 32) | @as(u64, index);
    }
};
