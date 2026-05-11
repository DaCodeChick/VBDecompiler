// Data flow analysis framework
// Provides reaching definitions, use-def chains, liveness analysis, and dominance

const std = @import("std");
const pcode = @import("../ir/pcode_ops.zig");
const pcode_func = @import("../ir/pcode_function.zig");

const PCodeOp = pcode.PCodeOp;
const Varnode = pcode.Varnode;
const PCodeFunction = pcode_func.PCodeFunction;
const PCodeBlock = pcode_func.PCodeBlock;

/// Definition site - identifies where a varnode is defined
pub const Definition = struct {
    block_addr: u32, // Address of the block containing the definition
    op_index: usize, // Index of the operation within the block
    varnode: Varnode, // The varnode being defined

    pub fn format(self: Definition, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        try writer.print("def@0x{x}[{}]:{}", .{ self.block_addr, self.op_index, self.varnode });
    }
};

/// Use site - identifies where a varnode is used
pub const Use = struct {
    block_addr: u32, // Address of the block containing the use
    op_index: usize, // Index of the operation within the block
    varnode: Varnode, // The varnode being used
    operand_index: u8, // Which operand (0 or 1)

    pub fn format(self: Use, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        try writer.print("use@0x{x}[{}]:{} (op{})", .{ self.block_addr, self.op_index, self.varnode, self.operand_index });
    }
};

/// Varnode key for hash maps (normalizes varnodes for comparison)
pub const VarnodeKey = struct {
    space: pcode.AddressSpace,
    offset: u64,
    size: u32,

    pub fn fromVarnode(vn: Varnode) VarnodeKey {
        return .{
            .space = vn.space,
            .offset = vn.offset,
            .size = vn.size,
        };
    }

    pub fn eql(a: VarnodeKey, b: VarnodeKey) bool {
        return a.space == b.space and a.offset == b.offset and a.size == b.size;
    }

    pub fn hash(key: VarnodeKey) u64 {
        var hasher = std.hash.Wyhash.init(0);
        hasher.update(&[_]u8{@intFromEnum(key.space)});
        hasher.update(std.mem.asBytes(&key.offset));
        hasher.update(std.mem.asBytes(&key.size));
        return hasher.final();
    }
};

/// Reaching definitions for a single program point
/// Maps each varnode to the set of definitions that may reach it
pub const ReachingDefs = struct {
    defs: std.AutoHashMap(VarnodeKey, std.ArrayList(Definition)),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) ReachingDefs {
        return .{
            .defs = std.AutoHashMap(VarnodeKey, std.ArrayList(Definition)).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *ReachingDefs) void {
        var it = self.defs.valueIterator();
        while (it.next()) |def_list| {
            def_list.deinit(self.allocator);
        }
        self.defs.deinit();
    }

    /// Add a reaching definition for a varnode
    pub fn addDef(self: *ReachingDefs, vn: Varnode, def: Definition) !void {
        const key = VarnodeKey.fromVarnode(vn);
        const result = try self.defs.getOrPut(key);
        if (!result.found_existing) {
            result.value_ptr.* = .{ .items = &.{}, .capacity = 0 };
        }
        try result.value_ptr.append(self.allocator, def);
    }

    /// Get all reaching definitions for a varnode
    pub fn getDefs(self: *const ReachingDefs, vn: Varnode) ?[]const Definition {
        const key = VarnodeKey.fromVarnode(vn);
        if (self.defs.get(key)) |def_list| {
            return def_list.items;
        }
        return null;
    }

    /// Merge another ReachingDefs into this one (union operation)
    pub fn merge(self: *ReachingDefs, other: *const ReachingDefs) !void {
        var it = other.defs.iterator();
        while (it.next()) |entry| {
            const key = entry.key_ptr.*;
            const other_defs = entry.value_ptr.items;

            const result = try self.defs.getOrPut(key);
            if (!result.found_existing) {
                result.value_ptr.* = .{ .items = &.{}, .capacity = 0 };
            }

            // Add definitions that aren't already present
            for (other_defs) |def| {
                var found = false;
                for (result.value_ptr.items) |existing| {
                    if (existing.block_addr == def.block_addr and existing.op_index == def.op_index) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    try result.value_ptr.append(self.allocator, def);
                }
            }
        }
    }

    /// Clone this ReachingDefs
    pub fn clone(self: *const ReachingDefs) !ReachingDefs {
        var result = ReachingDefs.init(self.allocator);
        errdefer result.deinit();

        var it = self.defs.iterator();
        while (it.next()) |entry| {
            const key = entry.key_ptr.*;
            const defs = entry.value_ptr.items;

            var def_list: std.ArrayList(Definition) = .{ .items = &.{}, .capacity = 0 };
            try def_list.appendSlice(self.allocator, defs);
            try result.defs.put(key, def_list);
        }

        return result;
    }

    /// Kill definitions for a varnode (used when a new definition is created)
    pub fn killDef(self: *ReachingDefs, vn: Varnode) void {
        const key = VarnodeKey.fromVarnode(vn);
        if (self.defs.getPtr(key)) |def_list| {
            def_list.clearRetainingCapacity();
        }
    }
};

/// Use-def chain: maps each use to its reaching definitions
pub const UseDefChain = struct {
    chains: std.AutoHashMap(Use, std.ArrayList(Definition)),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) UseDefChain {
        return .{
            .chains = std.AutoHashMap(Use, std.ArrayList(Definition)).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *UseDefChain) void {
        var it = self.chains.valueIterator();
        while (it.next()) |def_list| {
            def_list.deinit(self.allocator);
        }
        self.chains.deinit();
    }

    /// Add a use-def link
    pub fn addLink(self: *UseDefChain, use: Use, def: Definition) !void {
        const result = try self.chains.getOrPut(use);
        if (!result.found_existing) {
            result.value_ptr.* = .{ .items = &.{}, .capacity = 0 };
        }
        try result.value_ptr.append(self.allocator, def);
    }

    /// Get all definitions reaching a use
    pub fn getDefs(self: *const UseDefChain, use: Use) ?[]const Definition {
        if (self.chains.get(use)) |def_list| {
            return def_list.items;
        }
        return null;
    }
};

/// Def-use chain: maps each definition to all its uses
pub const DefUseChain = struct {
    chains: std.AutoHashMap(Definition, std.ArrayList(Use)),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) DefUseChain {
        return .{
            .chains = std.AutoHashMap(Definition, std.ArrayList(Use)).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *DefUseChain) void {
        var it = self.chains.valueIterator();
        while (it.next()) |use_list| {
            use_list.deinit(self.allocator);
        }
        self.chains.deinit();
    }

    /// Add a def-use link
    pub fn addLink(self: *DefUseChain, def: Definition, use: Use) !void {
        const result = try self.chains.getOrPut(def);
        if (!result.found_existing) {
            result.value_ptr.* = .{ .items = &.{}, .capacity = 0 };
        }
        try result.value_ptr.append(self.allocator, use);
    }

    /// Get all uses of a definition
    pub fn getUses(self: *const DefUseChain, def: Definition) ?[]const Use {
        if (self.chains.get(def)) |use_list| {
            return use_list.items;
        }
        return null;
    }
};

/// Live variable set - variables that are live at a program point
pub const LiveSet = struct {
    live: std.AutoHashMap(VarnodeKey, void),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) LiveSet {
        return .{
            .live = std.AutoHashMap(VarnodeKey, void).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *LiveSet) void {
        self.live.deinit();
    }

    /// Add a live variable
    pub fn add(self: *LiveSet, vn: Varnode) !void {
        const key = VarnodeKey.fromVarnode(vn);
        try self.live.put(key, {});
    }

    /// Remove a variable from the live set
    pub fn remove(self: *LiveSet, vn: Varnode) void {
        const key = VarnodeKey.fromVarnode(vn);
        _ = self.live.remove(key);
    }

    /// Check if a variable is live
    pub fn contains(self: *const LiveSet, vn: Varnode) bool {
        const key = VarnodeKey.fromVarnode(vn);
        return self.live.contains(key);
    }

    /// Merge another live set into this one (union)
    pub fn merge(self: *LiveSet, other: *const LiveSet) !void {
        var it = other.live.keyIterator();
        while (it.next()) |key| {
            try self.live.put(key.*, {});
        }
    }

    /// Clone this live set
    pub fn clone(self: *const LiveSet) !LiveSet {
        var result = LiveSet.init(self.allocator);
        errdefer result.deinit();
        try result.merge(self);
        return result;
    }

    /// Get count of live variables
    pub fn count(self: *const LiveSet) usize {
        return self.live.count();
    }
};

test "varnode key equality" {
    const key1 = VarnodeKey{ .space = .register, .offset = 0, .size = 4 };
    const key2 = VarnodeKey{ .space = .register, .offset = 0, .size = 4 };
    const key3 = VarnodeKey{ .space = .register, .offset = 1, .size = 4 };

    try std.testing.expect(VarnodeKey.eql(key1, key2));
    try std.testing.expect(!VarnodeKey.eql(key1, key3));
}

test "reaching definitions basic" {
    const allocator = std.testing.allocator;

    var reach = ReachingDefs.init(allocator);
    defer reach.deinit();

    const vn = Varnode.register(0, 4);
    const def = Definition{
        .block_addr = 0x1000,
        .op_index = 0,
        .varnode = vn,
    };

    try reach.addDef(vn, def);
    const defs = reach.getDefs(vn).?;
    try std.testing.expectEqual(@as(usize, 1), defs.len);
    try std.testing.expectEqual(@as(u32, 0x1000), defs[0].block_addr);
}

test "live set operations" {
    const allocator = std.testing.allocator;

    var live = LiveSet.init(allocator);
    defer live.deinit();

    const vn1 = Varnode.register(0, 4);
    const vn2 = Varnode.register(1, 4);

    try live.add(vn1);
    try std.testing.expect(live.contains(vn1));
    try std.testing.expect(!live.contains(vn2));

    try live.add(vn2);
    try std.testing.expectEqual(@as(usize, 2), live.count());

    live.remove(vn1);
    try std.testing.expect(!live.contains(vn1));
    try std.testing.expect(live.contains(vn2));
}
