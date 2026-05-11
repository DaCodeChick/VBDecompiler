// Type Inference Engine - Infer types for SSA values using constraint solving
const std = @import("std");
const VBType = @import("type_lattice.zig").VBType;
const TypeLattice = @import("type_lattice.zig").TypeLattice;
const TypeContext = @import("type_lattice.zig").TypeContext;
const TypeConstraint = @import("type_constraints.zig").TypeConstraint;
const ConstraintCollector = @import("type_constraints.zig").ConstraintCollector;
const VarRef = @import("type_constraints.zig").VarRef;
const SSAFunction = @import("../ir/ssa.zig").SSAFunction;

/// Type inference engine
pub const TypeInferenceEngine = struct {
    allocator: std.mem.Allocator,
    type_context: TypeContext,
    
    /// Inferred types for each variable
    var_types: std.AutoHashMap(u64, VBType),
    
    /// Constraints to solve
    constraints: std.ArrayList(TypeConstraint),
    
    /// Worklist for iterative solving
    worklist: std.ArrayList(u64),
    
    pub fn init(allocator: std.mem.Allocator) TypeInferenceEngine {
        return .{
            .allocator = allocator,
            .type_context = TypeContext.init(allocator),
            .var_types = std.AutoHashMap(u64, VBType).init(allocator),
            .constraints = std.ArrayList(TypeConstraint).empty,
            .worklist = std.ArrayList(u64).empty,
        };
    }
    
    pub fn deinit(self: *TypeInferenceEngine) void {
        self.var_types.deinit();
        self.constraints.deinit(self.allocator);
        self.worklist.deinit(self.allocator);
    }
    
    /// Infer types for an SSA function
    pub fn inferFunction(self: *TypeInferenceEngine, ssa_func: *const SSAFunction) !void {
        // Step 1: Collect constraints
        var collector = ConstraintCollector.init(self.allocator);
        defer collector.deinit();
        
        try collector.collectFromFunction(ssa_func);
        
        // Transfer constraints to engine
        for (collector.constraints.items) |constraint| {
            try self.constraints.append(self.allocator, constraint);
        }
        
        // Step 2: Initialize all variables to bottom type
        try self.initializeTypes();
        
        // Step 3: Apply initial constraints
        try self.applyInitialConstraints();
        
        // Step 4: Iteratively propagate types until fixed point
        try self.propagateTypes();
    }
    
    /// Initialize all variables to bottom type
    fn initializeTypes(self: *TypeInferenceEngine) !void {
        for (self.constraints.items) |constraint| {
            switch (constraint) {
                .equal => |c| {
                    try self.setType(c.a, .bottom);
                    try self.setType(c.b, .bottom);
                },
                .exact => |c| {
                    try self.setType(c.var_ref, c.type);
                },
                .numeric => |v| {
                    try self.setType(v, .bottom);
                },
                .integral => |v| {
                    try self.setType(v, .bottom);
                },
                .binary_op => |c| {
                    try self.setType(c.result, .bottom);
                    try self.setType(c.left, .bottom);
                    try self.setType(c.right, .bottom);
                },
                .unary_op => |c| {
                    try self.setType(c.result, .bottom);
                    try self.setType(c.operand, .bottom);
                },
                .load => |c| {
                    try self.setType(c.result, .bottom);
                    try self.setType(c.address, .bottom);
                },
                .store => |c| {
                    try self.setType(c.address, .bottom);
                    try self.setType(c.value, .bottom);
                },
                .call => |c| {
                    if (c.result) |result| {
                        try self.setType(result, .bottom);
                    }
                    try self.setType(c.target, .bottom);
                    for (c.args) |arg| {
                        try self.setType(arg, .bottom);
                    }
                },
            }
        }
    }
    
    /// Apply constraints that give us exact type information
    fn applyInitialConstraints(self: *TypeInferenceEngine) !void {
        for (self.constraints.items) |constraint| {
            switch (constraint) {
                .exact => |c| {
                    const current = self.getType(c.var_ref);
                    const new_type = TypeLattice.join(current, c.type);
                    if (!std.meta.eql(current, new_type)) {
                        try self.setType(c.var_ref, new_type);
                        try self.addToWorklist(c.var_ref);
                    }
                },
                .integral => |v| {
                    // Start with integer as default integral type
                    const current = self.getType(v);
                    if (current == .bottom) {
                        try self.setType(v, .integer);
                        try self.addToWorklist(v);
                    }
                },
                .numeric => |v| {
                    // Start with long as default numeric type
                    const current = self.getType(v);
                    if (current == .bottom) {
                        try self.setType(v, .long);
                        try self.addToWorklist(v);
                    }
                },
                else => {},
            }
        }
    }
    
    /// Iteratively propagate types through constraints
    fn propagateTypes(self: *TypeInferenceEngine) !void {
        var iterations: u32 = 0;
        const max_iterations: u32 = 1000;
        
        while (self.worklist.items.len > 0 and iterations < max_iterations) : (iterations += 1) {
            const var_hash = self.worklist.pop() orelse break;
            
            // Find constraints involving this variable
            for (self.constraints.items) |constraint| {
                try self.propagateConstraint(constraint, var_hash);
            }
        }
        
        if (iterations >= max_iterations) {
            std.log.warn("Type inference reached maximum iterations", .{});
        }
    }
    
    /// Propagate a single constraint
    fn propagateConstraint(self: *TypeInferenceEngine, constraint: TypeConstraint, changed_var: u64) !void {
        switch (constraint) {
            .equal => |c| {
                const a_hash = c.a.hash();
                const b_hash = c.b.hash();
                
                if (a_hash == changed_var or b_hash == changed_var) {
                    const a_type = self.getType(c.a);
                    const b_type = self.getType(c.b);
                    const joined = TypeLattice.join(a_type, b_type);
                    
                    if (!std.meta.eql(a_type, joined)) {
                        try self.setType(c.a, joined);
                        try self.addToWorklist(c.a);
                    }
                    if (!std.meta.eql(b_type, joined)) {
                        try self.setType(c.b, joined);
                        try self.addToWorklist(c.b);
                    }
                }
            },
            
            .binary_op => |c| {
                const result_hash = c.result.hash();
                const left_hash = c.left.hash();
                const right_hash = c.right.hash();
                
                if (left_hash == changed_var or right_hash == changed_var) {
                    const left_type = self.getType(c.left);
                    const right_type = self.getType(c.right);
                    
                    // Result type is promotion of operand types
                    const result_type = switch (c.op) {
                        .add, .sub, .mul, .div => TypeLattice.join(left_type, right_type),
                        .compare => VBType.boolean,
                        .and_op, .or_op, .xor => blk: {
                            // Bitwise ops preserve integral types
                            if (left_type.isIntegral() and right_type.isIntegral()) {
                                break :blk TypeLattice.join(left_type, right_type);
                            }
                            break :blk .long;
                        },
                        .shl, .shr => left_type,  // Shift preserves left operand type
                        .mod_op => TypeLattice.join(left_type, right_type),
                    };
                    
                    const current_result = self.getType(c.result);
                    const new_result = TypeLattice.join(current_result, result_type);
                    
                    if (!std.meta.eql(current_result, new_result)) {
                        try self.setType(c.result, new_result);
                        try self.addToWorklist(c.result);
                    }
                }
                
                // Backward propagation: if result has a type, constrain operands
                if (result_hash == changed_var) {
                    const result_type = self.getType(c.result);
                    if (result_type != .bottom and result_type != .top) {
                        const left_type = self.getType(c.left);
                        const right_type = self.getType(c.right);
                        
                        // Constrain operands based on result
                        const new_left = TypeLattice.meet(left_type, result_type);
                        const new_right = TypeLattice.meet(right_type, result_type);
                        
                        if (!std.meta.eql(left_type, new_left) and new_left != .bottom) {
                            try self.setType(c.left, new_left);
                            try self.addToWorklist(c.left);
                        }
                        if (!std.meta.eql(right_type, new_right) and new_right != .bottom) {
                            try self.setType(c.right, new_right);
                            try self.addToWorklist(c.right);
                        }
                    }
                }
            },
            
            .unary_op => |c| {
                _ = c.result.hash();
                const operand_hash = c.operand.hash();
                
                if (operand_hash == changed_var) {
                    const operand_type = self.getType(c.operand);
                    const result_type = switch (c.op) {
                        .neg, .not => operand_type,  // Preserve type
                        .convert => .variant,  // Conservative for conversions
                    };
                    
                    const current_result = self.getType(c.result);
                    const new_result = TypeLattice.join(current_result, result_type);
                    
                    if (!std.meta.eql(current_result, new_result)) {
                        try self.setType(c.result, new_result);
                        try self.addToWorklist(c.result);
                    }
                }
            },
            
            .exact => |c| {
                // Exact type constraint: directly set the type
                const var_hash = c.var_ref.hash();
                if (var_hash == changed_var or changed_var == 0) {
                    const current_type = self.getType(c.var_ref);
                    const new_type = TypeLattice.join(current_type, c.type);
                    
                    if (!std.meta.eql(current_type, new_type)) {
                        try self.setType(c.var_ref, new_type);
                        try self.addToWorklist(c.var_ref);
                    }
                }
            },
            
            .numeric => |var_ref| {
                // Numeric constraint: constrain to numeric types
                const var_hash = var_ref.hash();
                if (var_hash == changed_var or changed_var == 0) {
                    const current_type = self.getType(var_ref);
                    // If bottom, promote to integer as default numeric type
                    if (current_type == .bottom) {
                        try self.setType(var_ref, .integer);
                        try self.addToWorklist(var_ref);
                    }
                }
            },
            
            .integral => |var_ref| {
                // Integral constraint: constrain to integral types
                const var_hash = var_ref.hash();
                if (var_hash == changed_var or changed_var == 0) {
                    const current_type = self.getType(var_ref);
                    // If bottom, promote to integer as default integral type
                    if (current_type == .bottom) {
                        try self.setType(var_ref, .integer);
                        try self.addToWorklist(var_ref);
                    }
                }
            },
            
            .load => |c| {
                // Load operations: type is determined by size
                // Already handled in initial constraints
                _ = c;
            },
            
            .store => {
                // Store operations: ensure value type matches size
                // Could add more sophisticated handling here
            },
            
            .call => {
                // Call operations: would need function signature information
                // For now, assume Variant return type
            },
        }
    }
    
    /// Get the current type for a variable
    pub fn getType(self: *TypeInferenceEngine, var_ref: VarRef) VBType {
        const hash = var_ref.hash();
        return self.var_types.get(hash) orelse .bottom;
    }
    
    /// Set the type for a variable
    fn setType(self: *TypeInferenceEngine, var_ref: VarRef, vb_type: VBType) !void {
        const hash = var_ref.hash();
        try self.var_types.put(hash, vb_type);
    }
    
    /// Add a variable to the worklist
    fn addToWorklist(self: *TypeInferenceEngine, var_ref: VarRef) !void {
        const hash = var_ref.hash();
        // Check if already in worklist to avoid duplicates
        for (self.worklist.items) |item| {
            if (item == hash) return;
        }
        try self.worklist.append(self.allocator, hash);
    }
    
    /// Get all inferred types as a map
    pub fn getInferredTypes(self: *const TypeInferenceEngine) *const std.AutoHashMap(u64, VBType) {
        return &self.var_types;
    }
    
    /// Print inference statistics
    pub fn printStats(self: *const TypeInferenceEngine) void {
        var bottom_count: u32 = 0;
        var top_count: u32 = 0;
        var concrete_count: u32 = 0;
        
        var iter = self.var_types.valueIterator();
        while (iter.next()) |vb_type| {
            switch (vb_type.*) {
                .bottom => bottom_count += 1,
                .top => top_count += 1,
                else => concrete_count += 1,
            }
        }
        
        std.log.info("Type inference stats:", .{});
        std.log.info("  Total variables: {}", .{self.var_types.count()});
        std.log.info("  Concrete types: {}", .{concrete_count});
        std.log.info("  Unknown (⊥): {}", .{bottom_count});
        std.log.info("  Conflicts (⊤): {}", .{top_count});
    }
};

test "Type inference initialization" {
    const allocator = std.testing.allocator;
    var engine = TypeInferenceEngine.init(allocator);
    defer engine.deinit();
    
    const var1 = VarRef{ .block_id = 0, .index = 0 };
    try engine.setType(var1, .integer);
    
    const result = engine.getType(var1);
    try std.testing.expect(result == .integer);
}

test "Type propagation through equality" {
    const allocator = std.testing.allocator;
    var engine = TypeInferenceEngine.init(allocator);
    defer engine.deinit();
    
    const var1 = VarRef{ .block_id = 0, .index = 0 };
    const var2 = VarRef{ .block_id = 0, .index = 1 };
    
    // Add equality constraint
    try engine.constraints.append(allocator, .{ .equal = .{ .a = var1, .b = var2 } });
    
    // Set type for var1
    try engine.setType(var1, .integer);
    try engine.setType(var2, .bottom);
    try engine.addToWorklist(var1);
    
    // Propagate
    try engine.propagateTypes();
    
    // var2 should now be integer
    const result = engine.getType(var2);
    try std.testing.expect(result == .integer);
}
