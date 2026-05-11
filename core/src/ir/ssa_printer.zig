// SSA form printer for debugging and visualization

const std = @import("std");
const ssa_mod = @import("ssa.zig");
const pcode_ops = @import("pcode_ops.zig");

const SSAFunction = ssa_mod.SSAFunction;
const SSABlock = ssa_mod.SSABlock;
const SSAVarnode = ssa_mod.SSAVarnode;
const PhiNode = ssa_mod.PhiNode;
const SSAInstruction = ssa_mod.SSAInstruction;
const PCodeOp = pcode_ops.PCodeOp;

pub const SSAPrinterOptions = struct {
    color: bool = true,
    show_addresses: bool = true,
    indent: usize = 2,
};

pub const SSAPrinter = struct {
    allocator: std.mem.Allocator,
    options: SSAPrinterOptions,

    pub fn init(allocator: std.mem.Allocator, options: SSAPrinterOptions) SSAPrinter {
        return .{
            .allocator = allocator,
            .options = options,
        };
    }

    /// Print entire SSA function
    pub fn printFunction(self: *const SSAPrinter, writer: anytype, func: *const SSAFunction) !void {
        if (self.options.color) {
            try writer.writeAll("\x1b[1;32m"); // Bold green
        }
        try writer.print("=== SSA Function (entry: 0x{x:0>8}) ===\n\n", .{func.entry});
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        // Get sorted block addresses
        var block_addrs: std.ArrayList(u32) = .empty;
        defer block_addrs.deinit(self.allocator);

        var it = func.blocks.keyIterator();
        while (it.next()) |addr| {
            try block_addrs.append(self.allocator, addr.*);
        }
        std.mem.sort(u32, block_addrs.items, {}, comptime std.sort.asc(u32));

        // Print each block
        for (block_addrs.items) |block_addr| {
            if (func.blocks.get(block_addr)) |block| {
                try self.printBlock(writer, &block);
                try writer.writeAll("\n");
            }
        }
    }

    /// Print a single SSA block
    pub fn printBlock(self: *const SSAPrinter, writer: anytype, block: *const SSABlock) !void {
        // Block header
        if (self.options.color) {
            try writer.writeAll("\x1b[1;33m"); // Bold yellow
        }
        try writer.print("Block 0x{x:0>8}:\n", .{block.address});
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        // Print phi nodes
        if (block.phi_nodes.items.len > 0) {
            if (self.options.color) {
                try writer.writeAll("\x1b[36m"); // Cyan
            }
            try writer.writeAll("  Phi nodes:\n");
            if (self.options.color) {
                try writer.writeAll("\x1b[0m");
            }

            for (block.phi_nodes.items) |phi| {
                try self.printPhiNode(writer, &phi);
            }
            try writer.writeAll("\n");
        }

        // Print instructions
        if (block.instructions.items.len > 0) {
            if (self.options.color) {
                try writer.writeAll("\x1b[35m"); // Magenta
            }
            try writer.writeAll("  Instructions:\n");
            if (self.options.color) {
                try writer.writeAll("\x1b[0m");
            }

            for (block.instructions.items) |inst| {
                try self.printInstruction(writer, &inst);
            }
        }
    }

    /// Print a phi node
    fn printPhiNode(_: *const SSAPrinter, writer: anytype, phi: *const PhiNode) !void {
        const indent = " " ** 4;
        try writer.writeAll(indent);

        // Output
        try writer.print("{} = PHI(", .{phi.output});

        // Inputs
        for (phi.inputs.items, 0..) |input, i| {
            if (i > 0) try writer.writeAll(", ");
            try writer.print("{} from 0x{x:0>8}", .{ input.varnode, input.predecessor });
        }

        try writer.writeAll(")\n");
    }

    /// Print an SSA instruction
    fn printInstruction(self: *const SSAPrinter, writer: anytype, inst: *const SSAInstruction) !void {
        const indent = " " ** 4;
        try writer.writeAll(indent);

        // Show address if requested
        if (self.options.show_addresses) {
            if (self.options.color) {
                try writer.writeAll("\x1b[90m"); // Dark gray
            }
            try writer.print("[0x{x:0>8}] ", .{inst.address});
            if (self.options.color) {
                try writer.writeAll("\x1b[0m");
            }
        }

        // Output = op inputs
        if (inst.output) |output| {
            try writer.print("{} = ", .{output});
        }

        // Operation
        if (self.options.color) {
            try writer.writeAll("\x1b[1m"); // Bold
        }
        try writer.print("{s}", .{@tagName(inst.opcode)});
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        // Inputs
        if (inst.input0) |input0| {
            try writer.print(" {}", .{input0});
            if (inst.input1) |input1| {
                try writer.print(", {}", .{input1});
            }
        }

        try writer.writeAll("\n");
    }

    /// Print SSA statistics
    pub fn printStats(self: *const SSAPrinter, writer: anytype, func: *const SSAFunction) !void {
        var total_phi_nodes: usize = 0;
        var total_instructions: usize = 0;
        var max_version: u32 = 0;

        var it = func.blocks.valueIterator();
        while (it.next()) |block| {
            total_phi_nodes += block.phi_nodes.items.len;
            total_instructions += block.instructions.items.len;

            // Find max version
            for (block.phi_nodes.items) |phi| {
                if (phi.output.version > max_version) {
                    max_version = phi.output.version;
                }
            }

            for (block.instructions.items) |inst| {
                if (inst.output) |output| {
                    if (output.version > max_version) {
                        max_version = output.version;
                    }
                }
                if (inst.input0) |input| {
                    if (input.version > max_version) {
                        max_version = input.version;
                    }
                }
                if (inst.input1) |input| {
                    if (input.version > max_version) {
                        max_version = input.version;
                    }
                }
            }
        }

        if (self.options.color) {
            try writer.writeAll("\x1b[1;34m"); // Bold blue
        }
        try writer.writeAll("\n=== SSA Statistics ===\n");
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        try writer.print("Blocks:        {d}\n", .{func.blocks.count()});
        try writer.print("Phi nodes:     {d}\n", .{total_phi_nodes});
        try writer.print("Instructions:  {d}\n", .{total_instructions});
        try writer.print("Max version:   {d}\n", .{max_version});
    }
};
