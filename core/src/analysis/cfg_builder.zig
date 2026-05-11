// CFG Builder - constructs control flow graph through recursive traversal
const std = @import("std");
const cfg_mod = @import("cfg.zig");
const CFG = cfg_mod.CFG;
const BasicBlock = cfg_mod.BasicBlock;
const Edge = cfg_mod.Edge;
const EdgeType = cfg_mod.EdgeType;
const XRef = cfg_mod.XRef;
const XRefType = cfg_mod.XRefType;
const Function = cfg_mod.Function;
const CallingConvention = cfg_mod.CallingConvention;
const Decoder = @import("../disasm/decoder.zig").Decoder;
const Instruction = @import("../disasm/instruction.zig").Instruction;
const Mnemonic = @import("../disasm/instruction.zig").Mnemonic;

pub const CFGBuilderError = error{
    OutOfMemory,
    InvalidAddress,
    DecodeFailed,
};

/// CFG Builder options
pub const CFGBuilderOptions = struct {
    /// Maximum number of instructions to process (0 = unlimited)
    max_instructions: usize = 0,
    /// Maximum recursion depth
    max_depth: usize = 1000,
    /// Follow calls into other functions
    follow_calls: bool = false,
};

/// CFG Builder state
pub const CFGBuilder = struct {
    allocator: std.mem.Allocator,
    cfg: *CFG,
    data: []const u8,
    base_address: u32,
    options: CFGBuilderOptions,
    
    /// Addresses we need to visit
    worklist: std.ArrayList(u32),
    /// Addresses we've already visited
    visited: std.AutoHashMap(u32, void),
    /// Basic block leaders (addresses that start blocks)
    leaders: std.AutoHashMap(u32, void),
    /// All decoded instructions by address
    instructions: std.AutoHashMap(u32, Instruction),
    
    pub fn init(allocator: std.mem.Allocator, cfg: *CFG, data: []const u8, base_address: u32, options: CFGBuilderOptions) CFGBuilder {
        return CFGBuilder{
            .allocator = allocator,
            .cfg = cfg,
            .data = data,
            .base_address = base_address,
            .options = options,
            .worklist = .{ .items = &.{}, .capacity = 0 },
            .visited = std.AutoHashMap(u32, void).init(allocator),
            .leaders = std.AutoHashMap(u32, void).init(allocator),
            .instructions = std.AutoHashMap(u32, Instruction).init(allocator),
        };
    }
    
    pub fn deinit(self: *CFGBuilder) void {
        self.worklist.deinit(self.allocator);
        self.visited.deinit();
        self.leaders.deinit();
        self.instructions.deinit();
    }
    
    /// Build CFG starting from a function entry point
    pub fn buildFunction(self: *CFGBuilder, entry_point: u32) CFGBuilderError!void {
        // Add entry point to worklist and mark as leader
        try self.worklist.append(self.allocator, entry_point);
        try self.leaders.put(entry_point, {});
        
        // Process worklist until empty
        while (self.worklist.pop()) |address| {
            if (self.visited.contains(address)) {
                continue;
            }
            
            try self.processAddress(address);
        }
        
        // Build basic blocks from decoded instructions
        try self.buildBasicBlocks();
        
        // Identify function boundaries
        try self.identifyFunction(entry_point);
    }
    
    /// Process instructions starting at address
    fn processAddress(self: *CFGBuilder, address: u32) CFGBuilderError!void {
        if (address < self.base_address) return CFGBuilderError.InvalidAddress;
        
        const offset = address - self.base_address;
        if (offset >= self.data.len) return CFGBuilderError.InvalidAddress;
        
        var decoder = Decoder.init(self.data[offset..], address);
        
        // Decode instructions until we hit a terminator or visited address
        while (decoder.pos < decoder.data.len) {
            const inst_address = decoder.address + @as(u32, @intCast(decoder.pos));
            
            // If we've already visited this address, stop
            if (self.visited.contains(inst_address)) {
                break;
            }
            
            // Decode instruction
            const inst = decoder.decode() catch {
                // On decode error, stop processing this path
                break;
            };
            
            // Store instruction
            try self.instructions.put(inst_address, inst);
            try self.visited.put(inst_address, {});
            
            // Handle control flow
            const next_address = inst_address + inst.length;
            
            if (CFG.isBlockTerminator(inst.mnemonic)) {
                // Mark next instruction as a leader (if any)
                if (next_address < self.base_address + self.data.len) {
                    try self.leaders.put(next_address, {});
                }
                
                // Handle branches
                if (CFG.isBranch(inst.mnemonic)) {
                    // Get branch target
                    if (inst.operands[0] == .relative) {
                        const target: u32 = @bitCast(inst.operands[0].relative);
                        
                        // Add cross-reference
                        try self.cfg.addXRef(.{
                            .from = inst_address,
                            .to = target,
                            .type = .jump,
                        });
                        
                        // Mark target as leader
                        try self.leaders.put(target, {});
                        
                        // Add to worklist
                        if (!self.visited.contains(target)) {
                            try self.worklist.append(self.allocator, target);
                        }
                        
                        // For conditional branches, also follow fall-through
                        if (inst.mnemonic != .jmp) {
                            if (!self.visited.contains(next_address)) {
                                try self.worklist.append(self.allocator, next_address);
                            }
                        }
                    }
                } else if (CFG.isCall(inst.mnemonic)) {
                    // Get call target
                    if (inst.operands[0] == .relative) {
                        const target: u32 = @bitCast(inst.operands[0].relative);
                        
                        // Add cross-reference
                        try self.cfg.addXRef(.{
                            .from = inst_address,
                            .to = target,
                            .type = .call,
                        });
                        
                        // Mark target as function entry if following calls
                        if (self.options.follow_calls) {
                            try self.leaders.put(target, {});
                            if (!self.visited.contains(target)) {
                                try self.worklist.append(self.allocator, target);
                            }
                        }
                    }
                    
                    // Continue to next instruction after call
                    if (!self.visited.contains(next_address)) {
                        try self.worklist.append(self.allocator, next_address);
                    }
                } else if (CFG.isReturn(inst.mnemonic)) {
                    // Return terminates this path
                }
                
                // Stop processing this linear sequence
                break;
            }
        }
    }
    
    /// Build basic blocks from decoded instructions and leaders
    fn buildBasicBlocks(self: *CFGBuilder) CFGBuilderError!void {
        // Sort all instruction addresses
        var addresses: std.ArrayList(u32) = .{ .items = &.{}, .capacity = 0 };
        defer addresses.deinit(self.allocator);
        
        var inst_iter = self.instructions.keyIterator();
        while (inst_iter.next()) |addr| {
            try addresses.append(self.allocator, addr.*);
        }
        
        std.mem.sort(u32, addresses.items, {}, std.sort.asc(u32));
        
        if (addresses.items.len == 0) return;
        
        // Build blocks between leaders
        var block_start: ?u32 = null;
        var block_instructions: std.ArrayList(Instruction) = .{ .items = &.{}, .capacity = 0 };
        defer block_instructions.deinit(self.allocator);
        
        for (addresses.items) |addr| {
            const inst = self.instructions.get(addr).?;
            
            // Start new block if this is a leader
            if (self.leaders.contains(addr)) {
                // Finish previous block if any
                if (block_start) |start| {
                    try self.finishBlock(start, addr - 1, &block_instructions);
                    block_instructions.clearRetainingCapacity();
                }
                block_start = addr;
            }
            
            try block_instructions.append(self.allocator, inst);
            
            // Finish block if this is a terminator
            if (CFG.isBlockTerminator(inst.mnemonic)) {
                if (block_start) |start| {
                    try self.finishBlock(start, addr, &block_instructions);
                    block_instructions.clearRetainingCapacity();
                }
                block_start = null;
            }
        }
        
        // Finish last block if any
        if (block_start) |start| {
            const last_addr = addresses.items[addresses.items.len - 1];
            try self.finishBlock(start, last_addr, &block_instructions);
        }
    }
    
    /// Create a basic block and add it to the CFG
    fn finishBlock(self: *CFGBuilder, start: u32, end: u32, instructions: *std.ArrayList(Instruction)) CFGBuilderError!void {
        if (instructions.items.len == 0) return;
        
        // Build successors list
        var successors: std.ArrayList(Edge) = .{ .items = &.{}, .capacity = 0 };
        defer successors.deinit(self.allocator);
        
        const last_inst = instructions.items[instructions.items.len - 1];
        const next_addr = last_inst.address + last_inst.length;
        
        if (CFG.isBranch(last_inst.mnemonic)) {
            if (last_inst.operands[0] == .relative) {
                const target: u32 = @bitCast(last_inst.operands[0].relative);
                
                if (last_inst.mnemonic == .jmp) {
                    try successors.append(self.allocator, .{
                        .from = start,
                        .to = target,
                        .type = .unconditional_jump,
                    });
                } else {
                    try successors.append(self.allocator, .{
                        .from = start,
                        .to = target,
                        .type = .conditional_jump,
                    });
                    try successors.append(self.allocator, .{
                        .from = start,
                        .to = next_addr,
                        .type = .conditional_fallthrough,
                    });
                }
            }
        } else if (CFG.isCall(last_inst.mnemonic)) {
            // Call falls through to next instruction
            try successors.append(self.allocator, .{
                .from = start,
                .to = next_addr,
                .type = .fallthrough,
            });
        } else if (!CFG.isReturn(last_inst.mnemonic)) {
            // Normal fall-through
            try successors.append(self.allocator, .{
                .from = start,
                .to = next_addr,
                .type = .fallthrough,
            });
        }
        
        // Create basic block
        const block = BasicBlock{
            .start_address = start,
            .end_address = end,
            .instructions = try self.allocator.dupe(Instruction, instructions.items),
            .successors = try successors.toOwnedSlice(self.allocator),
            .predecessors = &.{}, // Will be filled in later
        };
        
        try self.cfg.addBlock(block);
    }
    
    /// Identify function boundaries and create function object
    fn identifyFunction(self: *CFGBuilder, entry_point: u32) CFGBuilderError!void {
        // Collect all blocks that belong to this function
        var func_blocks: std.ArrayList(u32) = .{ .items = &.{}, .capacity = 0 };
        defer func_blocks.deinit(self.allocator);
        
        var block_iter = self.cfg.blocks.keyIterator();
        while (block_iter.next()) |addr| {
            // Simple heuristic: all blocks we found belong to this function
            // TODO: More sophisticated analysis for multiple functions
            try func_blocks.append(self.allocator, addr.*);
        }
        
        // Calculate function size
        var max_addr: u32 = entry_point;
        for (func_blocks.items) |block_addr| {
            if (self.cfg.getBlock(block_addr)) |block| {
                if (block.end_address > max_addr) {
                    max_addr = block.end_address;
                }
            }
        }
        
        const func_size = max_addr - entry_point + 1;
        
        // Create function
        const func = Function{
            .address = entry_point,
            .size = func_size,
            .name = null,
            .blocks = try func_blocks.toOwnedSlice(self.allocator),
            .convention = .unknown, // TODO: Detect calling convention
            .is_export = false,
            .is_thunk = false,
        };
        
        try self.cfg.addFunction(func);
    }
};

test "CFG builder simple function" {
    // Simple function: push ebp, mov ebp esp, pop ebp, ret
    const data = [_]u8{
        0x55,             // push ebp
        0x89, 0xE5,       // mov ebp, esp
        0x5D,             // pop ebp
        0xC3,             // ret
    };
    
    var cfg = CFG.init(std.testing.allocator);
    defer cfg.deinit();
    
    var builder = CFGBuilder.init(std.testing.allocator, &cfg, &data, 0x401000, .{});
    defer builder.deinit();
    
    try builder.buildFunction(0x401000);
    
    // Should have one basic block
    try std.testing.expectEqual(@as(usize, 1), cfg.blocks.count());
    
    // Should have one function
    try std.testing.expectEqual(@as(usize, 1), cfg.functions.count());
    
    const func = cfg.getFunction(0x401000);
    try std.testing.expect(func != null);
    try std.testing.expectEqual(@as(u32, 0x401000), func.?.address);
}
