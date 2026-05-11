// Type Constraints - Collect and manage type constraints from SSA operations
const std = @import("std");
const VBType = @import("type_lattice.zig").VBType;
const TypeLattice = @import("type_lattice.zig").TypeLattice;
const PCodeOp = @import("../ir/pcode_ops.zig").PCodeOp;
const SSAFunction = @import("../ir/ssa.zig").SSAFunction;

/// Type constraint representing a relationship between types
pub const TypeConstraint = union(enum) {
    /// Type equality: a = b
    equal: struct {
        a: VarRef,
        b: VarRef,
    },
    
    /// Type must be a specific type
    exact: struct {
        var_ref: VarRef,
        type: VBType,
    },
    
    /// Type must be numeric
    numeric: VarRef,
    
    /// Type must be integral
    integral: VarRef,
    
    /// Binary operation constraint: result_type = op(left_type, right_type)
    binary_op: struct {
        result: VarRef,
        left: VarRef,
        right: VarRef,
        op: BinaryOp,
    },
    
    /// Unary operation constraint: result_type = op(operand_type)
    unary_op: struct {
        result: VarRef,
        operand: VarRef,
        op: UnaryOp,
    },
    
    /// Memory load constraint: result gets type from memory location
    load: struct {
        result: VarRef,
        address: VarRef,
        size: u32,
    },
    
    /// Memory store constraint: stored value must match memory type
    store: struct {
        address: VarRef,
        value: VarRef,
        size: u32,
    },
    
    /// Call constraint: arguments and return value
    call: struct {
        result: ?VarRef,
        target: VarRef,
        args: []VarRef,
    },
    
    pub const BinaryOp = enum {
        add,
        sub,
        mul,
        div,
        mod_op,
        and_op,
        or_op,
        xor,
        shl,
        shr,
        compare,
    };
    
    pub const UnaryOp = enum {
        neg,
        not,
        convert,
    };
};

/// Reference to a variable (SSA value)
pub const VarRef = struct {
    block_id: u32,
    index: u32,  // Index in the block's ops
    
    pub fn eql(a: VarRef, b: VarRef) bool {
        return a.block_id == b.block_id and a.index == b.index;
    }
    
    pub fn hash(self: VarRef) u64 {
        return (@as(u64, self.block_id) << 32) | self.index;
    }
};

/// Constraint collector - extracts type constraints from SSA
pub const ConstraintCollector = struct {
    allocator: std.mem.Allocator,
    constraints: std.ArrayList(TypeConstraint),
    
    pub fn init(allocator: std.mem.Allocator) ConstraintCollector {
        return .{
            .allocator = allocator,
            .constraints = std.ArrayList(TypeConstraint).empty,
        };
    }
    
    pub fn deinit(self: *ConstraintCollector) void {
        for (self.constraints.items) |constraint| {
            switch (constraint) {
                .call => |c| if (c.args.len > 0) self.allocator.free(c.args),
                else => {},
            }
        }
        self.constraints.deinit(self.allocator);
    }
    
    /// Collect constraints from an SSA function
    pub fn collectFromFunction(self: *ConstraintCollector, ssa_func: *const SSAFunction) !void {
        var it = ssa_func.blocks.valueIterator();
        while (it.next()) |block| {
            try self.collectFromBlock(block.*, block.address);
        }
    }
    
    /// Collect constraints from a single block
    fn collectFromBlock(self: *ConstraintCollector, block: anytype, block_id: u32) !void {
        // Process phi nodes
        for (block.phi_nodes.items, 0..) |phi, phi_idx| {
            _ = phi;
            _ = phi_idx;
            // TODO: Process phi nodes for type constraints
        }
        
        // Process instructions
        for (block.instructions.items, 0..) |inst, inst_idx| {
            try self.collectFromInstruction(inst, block_id, @intCast(inst_idx));
        }
    }
    
    /// Collect constraints from a single SSA instruction
    fn collectFromInstruction(self: *ConstraintCollector, inst: anytype, block_id: u32, inst_idx: u32) !void {
        const result_ref = VarRef{ .block_id = block_id, .index = inst_idx };
        
        switch (inst.opcode) {
            .int_add, .int_sub, .int_mult, .int_div => {
                // Integer arithmetic: all operands and result must be integral
                const left_ref = try self.makeVarRef(inst.input0);
                const right_ref = try self.makeVarRef(inst.input1);
                
                try self.constraints.append(self.allocator, .{ .integral = left_ref });
                try self.constraints.append(self.allocator, .{ .integral = right_ref });
                try self.constraints.append(self.allocator, .{ .binary_op = .{
                    .result = result_ref,
                    .left = left_ref,
                    .right = right_ref,
                    .op = switch (inst.opcode) {
                        .int_add => .add,
                        .int_sub => .sub,
                        .int_mult => .mul,
                        .int_div => .div,
                        else => unreachable,
                    },
                } });
            },
            
            // NOTE: Float operations not yet in P-code implementation
            // Will be added when float support is added
            
            .int_and, .int_or, .int_xor => {
                // Bitwise operations: integral types
                const left_ref = try self.makeVarRef(inst.input0);
                const right_ref = try self.makeVarRef(inst.input1);
                
                try self.constraints.append(self.allocator, .{ .integral = left_ref });
                try self.constraints.append(self.allocator, .{ .integral = right_ref });
                try self.constraints.append(self.allocator, .{ .binary_op = .{
                    .result = result_ref,
                    .left = left_ref,
                    .right = right_ref,
                    .op = switch (inst.opcode) {
                        .int_and => .and_op,
                        .int_or => .or_op,
                        .int_xor => .xor,
                        else => unreachable,
                    },
                } });
            },
            
            .int_neg, .int_not => {
                // Unary integer operations
                const operand_ref = try self.makeVarRef(inst.input0);
                try self.constraints.append(self.allocator, .{ .integral = operand_ref });
                try self.constraints.append(self.allocator, .{ .unary_op = .{
                    .result = result_ref,
                    .operand = operand_ref,
                    .op = .neg,
                } });
            },
            
            .load => {
                // Memory load: infer type from size
                const addr_ref = try self.makeVarRef(inst.input0);
                if (inst.output) |output| {
                    const size = output.size;
                    
                    try self.constraints.append(self.allocator, .{ .load = .{
                        .result = result_ref,
                        .address = addr_ref,
                        .size = size,
                    } });
                    
                    // Infer exact type from size
                    const inferred_type: VBType = switch (size) {
                        1 => .byte,
                        2 => .integer,  // Could be Boolean, but Integer is more common
                        4 => .long,     // Could be Single, Object, String
                        8 => .double_type,  // Could be Currency, Date
                        16 => .variant,
                        else => .variant,
                    };
                    
                    try self.constraints.append(self.allocator, .{ .exact = .{
                        .var_ref = result_ref,
                        .type = inferred_type,
                    } });
                }
            },
            
            .store => {
                // Memory store
                const addr_ref = try self.makeVarRef(inst.input0);
                const value_ref = try self.makeVarRef(inst.input1);
                if (inst.input1) |input1| {
                    const size = input1.size;
                    
                    try self.constraints.append(self.allocator, .{ .store = .{
                        .address = addr_ref,
                        .value = value_ref,
                        .size = size,
                    } });
                }
            },
            
            .copy => {
                // Copy: types must be equal
                const src_ref = try self.makeVarRef(inst.input0);
                try self.constraints.append(self.allocator, .{ .equal = .{
                    .a = result_ref,
                    .b = src_ref,
                } });
            },
            
            .int_less, .int_sless, .int_lessequal, .int_equal, .int_notequal => {
                // Comparison: operands should have compatible types, result is Boolean
                const left_ref = try self.makeVarRef(inst.input0);
                const right_ref = try self.makeVarRef(inst.input1);
                
                try self.constraints.append(self.allocator, .{ .binary_op = .{
                    .result = result_ref,
                    .left = left_ref,
                    .right = right_ref,
                    .op = .compare,
                } });
                
                // Result is boolean
                try self.constraints.append(self.allocator, .{ .exact = .{
                    .var_ref = result_ref,
                    .type = .boolean,
                } });
            },
            
            .call, .callind => {
                // Function call: complex constraint
                const target_ref = try self.makeVarRef(inst.input0);
                
                // Collect arguments (would need more context to get actual args)
                const args = try self.allocator.alloc(VarRef, 0);
                
                try self.constraints.append(self.allocator, .{ .call = .{
                    .result = result_ref,
                    .target = target_ref,
                    .args = args,
                } });
            },
            
            else => {
                // Other operations: no specific constraints yet
            },
        }
    }
    
    /// Helper to create a VarRef from a Varnode
    fn makeVarRef(self: *ConstraintCollector, varnode: anytype) !VarRef {
        _ = self;
        _ = varnode;
        // This would need proper implementation based on actual Varnode structure
        // For now, return a placeholder
        return VarRef{ .block_id = 0, .index = 0 };
    }
};

test "Constraint collection basics" {
    const allocator = std.testing.allocator;
    var collector = ConstraintCollector.init(allocator);
    defer collector.deinit();
    
    // Test that we can create constraints
    try collector.constraints.append(allocator, .{ .integral = .{ .block_id = 0, .index = 0 } });
    try collector.constraints.append(allocator, .{ .numeric = .{ .block_id = 0, .index = 1 } });
    
    try std.testing.expectEqual(@as(usize, 2), collector.constraints.items.len);
}
