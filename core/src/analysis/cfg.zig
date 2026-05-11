// Control Flow Graph (CFG) analysis
const std = @import("std");
const Instruction = @import("../disasm/instruction.zig").Instruction;
const Mnemonic = @import("../disasm/instruction.zig").Mnemonic;

/// Type of control flow edge
pub const EdgeType = enum {
    /// Unconditional fall-through to next instruction
    fallthrough,
    /// Unconditional jump
    unconditional_jump,
    /// Conditional jump (taken branch)
    conditional_jump,
    /// Conditional jump (not taken, fall through)
    conditional_fallthrough,
    /// Function call
    call,
    /// Return from function
    ret,
};

/// Control flow edge connecting two basic blocks
pub const Edge = struct {
    from: u32, // Address of source basic block
    to: u32,   // Address of destination basic block
    type: EdgeType,
};

/// A basic block - sequence of instructions with single entry and exit
pub const BasicBlock = struct {
    /// Start address of the basic block
    start_address: u32,
    /// End address (inclusive) of the basic block
    end_address: u32,
    /// Instructions in this basic block
    instructions: []Instruction,
    /// Outgoing edges from this block
    successors: []Edge,
    /// Incoming edges to this block
    predecessors: []u32, // Just addresses for simplicity
    
    pub fn deinit(self: *BasicBlock, allocator: std.mem.Allocator) void {
        allocator.free(self.instructions);
        allocator.free(self.successors);
        allocator.free(self.predecessors);
    }
};

/// Cross-reference types
pub const XRefType = enum {
    call,
    jump,
    data_read,
    data_write,
};

/// Cross-reference from one address to another
pub const XRef = struct {
    from: u32,
    to: u32,
    type: XRefType,
};

/// Function boundary information
pub const Function = struct {
    /// Entry point address
    address: u32,
    /// Size in bytes (0 if unknown)
    size: u32,
    /// Name (if known, e.g., from exports)
    name: ?[]const u8,
    /// Basic blocks in this function
    blocks: []u32, // Addresses of basic blocks
    /// Calling convention (if detected)
    convention: CallingConvention,
    /// Is this an exported function?
    is_export: bool,
    /// Is this a thunk (jump to another function)?
    is_thunk: bool,
    
    pub fn deinit(self: *Function, allocator: std.mem.Allocator) void {
        if (self.name) |name| {
            allocator.free(name);
        }
        allocator.free(self.blocks);
    }
};

/// Calling convention
pub const CallingConvention = enum {
    unknown,
    cdecl,
    stdcall,
    fastcall,
    thiscall,
};

/// Control Flow Graph for a binary
pub const CFG = struct {
    allocator: std.mem.Allocator,
    /// Map of address -> basic block
    blocks: std.AutoHashMap(u32, BasicBlock),
    /// Map of address -> function
    functions: std.AutoHashMap(u32, Function),
    /// All cross-references
    xrefs: std.ArrayList(XRef),
    
    pub fn init(allocator: std.mem.Allocator) CFG {
        return CFG{
            .allocator = allocator,
            .blocks = std.AutoHashMap(u32, BasicBlock).init(allocator),
            .functions = std.AutoHashMap(u32, Function).init(allocator),
            .xrefs = .{ .items = &.{}, .capacity = 0 },
        };
    }
    
    pub fn deinit(self: *CFG) void {
        // Free all basic blocks
        var block_iter = self.blocks.valueIterator();
        while (block_iter.next()) |block| {
            var b = block.*;
            b.deinit(self.allocator);
        }
        self.blocks.deinit();
        
        // Free all functions
        var func_iter = self.functions.valueIterator();
        while (func_iter.next()) |func| {
            var f = func.*;
            f.deinit(self.allocator);
        }
        self.functions.deinit();
        
        self.xrefs.deinit(self.allocator);
    }
    
    /// Add a basic block to the CFG
    pub fn addBlock(self: *CFG, block: BasicBlock) !void {
        try self.blocks.put(block.start_address, block);
    }
    
    /// Add a function to the CFG
    pub fn addFunction(self: *CFG, func: Function) !void {
        try self.functions.put(func.address, func);
    }
    
    /// Add a cross-reference
    pub fn addXRef(self: *CFG, xref: XRef) !void {
        try self.xrefs.append(self.allocator, xref);
    }
    
    /// Get basic block at address
    pub fn getBlock(self: *const CFG, address: u32) ?*const BasicBlock {
        return self.blocks.getPtr(address);
    }
    
    /// Get function at address
    pub fn getFunction(self: *const CFG, address: u32) ?*const Function {
        return self.functions.getPtr(address);
    }
    
    /// Get all cross-references to an address
    pub fn getXRefsTo(self: *const CFG, address: u32, allocator: std.mem.Allocator) ![]XRef {
        var result: std.ArrayList(XRef) = .{ .items = &.{}, .capacity = 0 };
        errdefer result.deinit(allocator);
        
        for (self.xrefs.items) |xref| {
            if (xref.to == address) {
                try result.append(allocator, xref);
            }
        }
        
        return result.toOwnedSlice(allocator);
    }
    
    /// Get all cross-references from an address
    pub fn getXRefsFrom(self: *const CFG, address: u32, allocator: std.mem.Allocator) ![]XRef {
        var result: std.ArrayList(XRef) = .{ .items = &.{}, .capacity = 0 };
        errdefer result.deinit(allocator);
        
        for (self.xrefs.items) |xref| {
            if (xref.from == address) {
                try result.append(allocator, xref);
            }
        }
        
        return result.toOwnedSlice(allocator);
    }
    
    /// Check if instruction is a branch/jump
    pub fn isBranch(mnemonic: Mnemonic) bool {
        return switch (mnemonic) {
            .jmp, .je, .jne, .jz, .jnz, .ja, .jae, .jb, .jbe,
            .jg, .jge, .jl, .jle, .js, .jns, .jo, .jno => true,
            else => false,
        };
    }
    
    /// Check if instruction is a call
    pub fn isCall(mnemonic: Mnemonic) bool {
        return mnemonic == .call;
    }
    
    /// Check if instruction is a return
    pub fn isReturn(mnemonic: Mnemonic) bool {
        return mnemonic == .ret or mnemonic == .retn or mnemonic == .retf;
    }
    
    /// Check if instruction terminates a basic block
    pub fn isBlockTerminator(mnemonic: Mnemonic) bool {
        return isBranch(mnemonic) or isCall(mnemonic) or isReturn(mnemonic);
    }
};

test "CFG basic operations" {
    var cfg = CFG.init(std.testing.allocator);
    defer cfg.deinit();
    
    // Create a simple basic block
    const block = BasicBlock{
        .start_address = 0x401000,
        .end_address = 0x401010,
        .instructions = &.{},
        .successors = &.{},
        .predecessors = &.{},
    };
    
    try cfg.addBlock(block);
    
    const found = cfg.getBlock(0x401000);
    try std.testing.expect(found != null);
    try std.testing.expectEqual(@as(u32, 0x401000), found.?.start_address);
}
