// P-code function and basic block representation
// Provides structured IR representation using P-code operations

const std = @import("std");
const pcode_ops = @import("pcode_ops.zig");
const x86_translator = @import("x86_translator.zig");
const cfg_mod = @import("../analysis/cfg.zig");
const inst_mod = @import("../disasm/instruction.zig");

const PCodeOp = pcode_ops.PCodeOp;
const Translator = x86_translator.Translator;
const CFG = cfg_mod.CFG;
const BasicBlock = cfg_mod.BasicBlock;
const Instruction = inst_mod.Instruction;

/// P-code basic block
/// Contains P-code operations for a single basic block
pub const PCodeBlock = struct {
    address: u32, // Start address of the block
    end_address: u32, // End address of the block
    ops: std.ArrayList(PCodeOp), // P-code operations in this block
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, address: u32) PCodeBlock {
        return .{
            .address = address,
            .end_address = address,
            .ops = .{ .items = &.{}, .capacity = 0 },
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *PCodeBlock) void {
        self.ops.deinit(self.allocator);
    }

    /// Add P-code operations to this block
    pub fn addOps(self: *PCodeBlock, new_ops: []const PCodeOp) !void {
        try self.ops.appendSlice(self.allocator, new_ops);
    }

    /// Format the P-code block for display
    pub fn format(self: *const PCodeBlock, writer: anytype) !void {
        try writer.print("Block 0x{x} - 0x{x}:\n", .{ self.address, self.end_address });
        for (self.ops.items) |op| {
            try writer.print("  {}\n", .{op});
        }
    }
};

/// P-code function
/// Contains P-code representation of an entire function with basic blocks
pub const PCodeFunction = struct {
    address: u32, // Entry point address
    name: []const u8, // Function name
    blocks: std.AutoHashMap(u32, PCodeBlock), // Map address -> block
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, address: u32, name: []const u8) PCodeFunction {
        return .{
            .address = address,
            .name = name,
            .blocks = std.AutoHashMap(u32, PCodeBlock).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *PCodeFunction) void {
        var it = self.blocks.valueIterator();
        while (it.next()) |block| {
            block.deinit();
        }
        self.blocks.deinit();
    }

    /// Get or create a block at the given address
    pub fn getOrCreateBlock(self: *PCodeFunction, address: u32) !*PCodeBlock {
        const result = try self.blocks.getOrPut(address);
        if (!result.found_existing) {
            result.value_ptr.* = PCodeBlock.init(self.allocator, address);
        }
        return result.value_ptr;
    }

    /// Format the P-code function for display
    pub fn format(self: *const PCodeFunction, writer: anytype) !void {
        try writer.print("Function {s} @ 0x{x}:\n", .{ self.name, self.address });
        try writer.writeAll("----------------------------------------\n");

        // Sort blocks by address for consistent output
        var addresses = std.ArrayList(u32).init(self.allocator);
        defer addresses.deinit();

        var it = self.blocks.keyIterator();
        while (it.next()) |addr| {
            try addresses.append(self.allocator, addr.*);
        }

        std.mem.sort(u32, addresses.items, {}, comptime std.sort.asc(u32));

        for (addresses.items) |addr| {
            const block = self.blocks.get(addr).?;
            try block.format(writer);
            try writer.writeAll("\n");
        }
    }
};

/// P-code builder
/// Translates from x86 CFG to P-code representation
pub const PCodeBuilder = struct {
    allocator: std.mem.Allocator,
    translator: Translator,

    pub fn init(allocator: std.mem.Allocator) PCodeBuilder {
        return .{
            .allocator = allocator,
            .translator = Translator.init(allocator),
        };
    }

    pub fn deinit(self: *PCodeBuilder) void {
        self.translator.deinit();
    }

    /// Build P-code representation for a function from CFG
    pub fn buildFunction(
        self: *PCodeBuilder,
        cfg: *const CFG,
        address: u32,
        instructions: []const Instruction,
    ) !PCodeFunction {
        const func = cfg.functions.get(address) orelse return error.FunctionNotFound;

        const func_name = func.name orelse "unknown";
        var pcode_func = PCodeFunction.init(self.allocator, address, func_name);
        errdefer pcode_func.deinit();

        // Build an instruction lookup map for quick access
        var inst_map = std.AutoHashMap(u32, Instruction).init(self.allocator);
        defer inst_map.deinit();

        for (instructions) |instr| {
            try inst_map.put(instr.address, instr);
        }

        // Process each basic block
        for (func.blocks) |bb_addr| {
            const bb = cfg.blocks.get(bb_addr) orelse continue;
            const pcode_block = try pcode_func.getOrCreateBlock(bb.start_address);
            pcode_block.end_address = bb.end_address;

            // Translate each instruction in the block to P-code
            var current_addr = bb.start_address;
            while (current_addr < bb.end_address) {
                if (inst_map.get(current_addr)) |instr| {
                    self.translator.clear();
                    try self.translator.translateInstruction(&instr);
                    try pcode_block.addOps(self.translator.getOps());
                    current_addr = instr.address + instr.length;
                } else {
                    // No instruction found at this address, skip
                    current_addr += 1;
                }
            }
        }

        return pcode_func;
    }

    /// Build P-code for a single basic block
    pub fn buildBlock(
        self: *PCodeBuilder,
        instructions: []const Instruction,
        start: u32,
        end: u32,
    ) !PCodeBlock {
        var block = PCodeBlock.init(self.allocator, start);
        errdefer block.deinit();

        block.end_address = end;

        // Translate each instruction
        for (instructions) |instr| {
            if (instr.address >= start and instr.address < end) {
                self.translator.clear();
                try self.translator.translateInstruction(&instr);
                try block.addOps(self.translator.getOps());
            }
        }

        return block;
    }
};

test "pcode block creation" {
    const allocator = std.testing.allocator;
    
    var block = PCodeBlock.init(allocator, 0x1000);
    defer block.deinit();

    try std.testing.expectEqual(@as(u32, 0x1000), block.address);
    try std.testing.expectEqual(@as(usize, 0), block.ops.items.len);
}

test "pcode function creation" {
    const allocator = std.testing.allocator;
    
    var func = PCodeFunction.init(allocator, 0x1000, "test_func");
    defer func.deinit();

    try std.testing.expectEqual(@as(u32, 0x1000), func.address);
    try std.testing.expectEqualStrings("test_func", func.name);
}

test "pcode builder simple instruction" {
    const allocator = std.testing.allocator;
    
    var builder = PCodeBuilder.init(allocator);
    defer builder.deinit();

    const instructions = [_]Instruction{
        .{
            .address = 0x1000,
            .length = 5,
            .mnemonic = .mov,
            .operands = .{
                .{ .register = .eax },
                .{ .immediate = 42 },
                .none,
            },
        },
        .{
            .address = 0x1005,
            .length = 1,
            .mnemonic = .ret,
            .operands = .{ .none, .none, .none },
        },
    };

    var block = try builder.buildBlock(&instructions, 0x1000, 0x1006);
    defer block.deinit();

    try std.testing.expect(block.ops.items.len > 0);
    try std.testing.expectEqual(@as(u32, 0x1000), block.address);
    try std.testing.expectEqual(@as(u32, 0x1006), block.end_address);
}
