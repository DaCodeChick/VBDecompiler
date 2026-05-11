// Expression Reconstruction - Converts SSA operations to VB6 expressions
// Builds expression trees from SSA instructions

const std = @import("std");
const SSAFunction = @import("../ir/ssa.zig").SSAFunction;
const SSABlock = @import("../ir/ssa.zig").SSABlock;
const SSAInstruction = @import("../ir/ssa.zig").SSAInstruction;
const SSAVarnode = @import("../ir/ssa.zig").SSAVarnode;
const PCodeOp = @import("../ir/pcode_ops.zig");
const OpCode = @import("../ir/pcode_ops.zig").OpCode;
const vb6_ast = @import("vb6_ast.zig");
const Expression = vb6_ast.Expression;
const Variable = vb6_ast.Variable;
const BinaryOp = vb6_ast.BinaryOp;
const BinaryOpKind = vb6_ast.BinaryOpKind;
const UnaryOp = vb6_ast.UnaryOp;
const UnaryOpKind = vb6_ast.UnaryOpKind;
const VBType = @import("../analysis/type_lattice.zig").VBType;
const TypeInferenceEngine = @import("../analysis/type_inference.zig").TypeInferenceEngine;

/// Expression reconstructor
pub const ExpressionReconstructor = struct {
    allocator: std.mem.Allocator,
    ssa_func: *const SSAFunction,
    type_engine: *const TypeInferenceEngine,
    var_names: std.AutoHashMap(u64, []const u8),
    
    pub fn init(allocator: std.mem.Allocator, ssa_func: *const SSAFunction, type_engine: *const TypeInferenceEngine) ExpressionReconstructor {
        return .{
            .allocator = allocator,
            .ssa_func = ssa_func,
            .type_engine = type_engine,
            .var_names = std.AutoHashMap(u64, []const u8).init(allocator),
        };
    }
    
    pub fn deinit(self: *ExpressionReconstructor) void {
        var it = self.var_names.valueIterator();
        while (it.next()) |name| {
            self.allocator.free(name.*);
        }
        self.var_names.deinit();
    }
    
    /// Reconstruct an expression from an SSA instruction
    pub fn reconstructExpression(self: *ExpressionReconstructor, inst: *const SSAInstruction) !*Expression {
        const expr = try self.allocator.create(Expression);
        
        switch (inst.opcode) {
            .copy => {
                // Simple copy - just reconstruct the input
                if (inst.input0) |input| {
                    expr.* = try self.reconstructVarnode(input);
                } else {
                    expr.* = .{ .integer_literal = 0 };
                }
            },
            
            .int_add, .int_sub, .int_mult, .int_div, .int_sdiv => {
                expr.* = try self.reconstructBinaryOp(inst);
            },
            
            .int_and, .int_or, .int_xor => {
                expr.* = try self.reconstructBinaryOp(inst);
            },
            
            .int_equal, .int_notequal, .int_less, .int_sless => {
                expr.* = try self.reconstructBinaryOp(inst);
            },
            
            .int_neg, .int_not => {
                expr.* = try self.reconstructUnaryOp(inst);
            },
            
            .load => {
                // Memory load - treat as variable reference
                expr.* = try self.reconstructLoad(inst);
            },
            
            .call, .callind => {
                // Function call
                expr.* = try self.reconstructCall(inst);
            },
            
            else => {
                // Unsupported or special instruction - create placeholder integer
                expr.* = .{ .integer_literal = 0 };
            },
        }
        
        return expr;
    }
    
    /// Reconstruct a varnode as an expression
    fn reconstructVarnode(self: *ExpressionReconstructor, varnode: SSAVarnode) !Expression {
        switch (varnode.space) {
            .constant => {
                // Constant value
                const value = varnode.offset;
                if (varnode.size <= 4) {
                    return Expression{ .integer_literal = @intCast(value) };
                } else {
                    return Expression{ .long_literal = @intCast(value) };
                }
            },
            
            .register, .unique, .ram, .stack => {
                // Variable reference
                const var_id = self.makeVarId(varnode);
                const var_name = try self.getOrCreateVarName(var_id);
                const vb_type = self.type_engine.var_types.get(var_id) orelse .variant;
                
                return Expression{
                    .variable = Variable{
                        .name = var_name,
                        .vb_type = vb_type,
                    },
                };
            },
        }
    }
    
    /// Reconstruct a binary operation
    fn reconstructBinaryOp(self: *ExpressionReconstructor, inst: *const SSAInstruction) !Expression {
        const left = try self.allocator.create(Expression);
        const right = try self.allocator.create(Expression);
        
        if (inst.input0) |input0| {
            left.* = try self.reconstructVarnode(input0);
        } else {
            left.* = .{ .integer_literal = 0 };
        }
        
        if (inst.input1) |input1| {
            right.* = try self.reconstructVarnode(input1);
        } else {
            right.* = .{ .integer_literal = 0 };
        }
        
        const op_kind = self.opcodeToVB6BinaryOp(inst.opcode);
        
        return Expression{
            .binary_op = BinaryOp{
                .op = op_kind,
                .left = left,
                .right = right,
            },
        };
    }
    
    /// Reconstruct a unary operation
    fn reconstructUnaryOp(self: *ExpressionReconstructor, inst: *const SSAInstruction) !Expression {
        const operand = try self.allocator.create(Expression);
        
        if (inst.input0) |input0| {
            operand.* = try self.reconstructVarnode(input0);
        } else {
            operand.* = .{ .integer_literal = 0 };
        }
        
        const op_kind = self.opcodeToVB6UnaryOp(inst.opcode);
        
        return Expression{
            .unary_op = UnaryOp{
                .op = op_kind,
                .operand = operand,
            },
        };
    }
    
    /// Reconstruct a load instruction
    fn reconstructLoad(self: *ExpressionReconstructor, inst: *const SSAInstruction) !Expression {
        // Simplified: treat as variable reference
        if (inst.input0) |input0| {
            return try self.reconstructVarnode(input0);
        }
        
        return Expression{ .integer_literal = 0 };
    }
    
    /// Reconstruct a call instruction
    fn reconstructCall(self: *ExpressionReconstructor, inst: *const SSAInstruction) !Expression {
        // Simplified: create a placeholder call
        const target_name = try std.fmt.allocPrint(self.allocator, "sub_{x}", .{inst.address});
        
        return Expression{
            .call = vb6_ast.Call{
                .target = target_name,
                .arguments = std.ArrayList(*Expression).empty,
            },
        };
    }
    
    /// Convert P-code opcode to VB6 binary operation
    fn opcodeToVB6BinaryOp(self: *ExpressionReconstructor, opcode: OpCode) BinaryOpKind {
        _ = self;
        return switch (opcode) {
            .int_add => .add,
            .int_sub => .sub,
            .int_mult => .mul,
            .int_div, .int_sdiv => .int_div,
            .int_and => .and_op,
            .int_or => .or_op,
            .int_xor => .xor,
            .int_equal => .eq,
            .int_notequal => .ne,
            .int_less, .int_sless => .lt,
            .int_lessequal, .int_slessequal => .le,
            else => .add, // Default fallback
        };
    }
    
    /// Convert P-code opcode to VB6 unary operation
    fn opcodeToVB6UnaryOp(self: *ExpressionReconstructor, opcode: OpCode) UnaryOpKind {
        _ = self;
        return switch (opcode) {
            .int_neg => .negate,
            .int_not, .bool_negate => .not_op,
            else => .negate, // Default fallback
        };
    }
    
    /// Generate a unique variable ID from a varnode
    fn makeVarId(self: *ExpressionReconstructor, varnode: SSAVarnode) u64 {
        _ = self;
        return (@as(u64, varnode.offset) << 32) | @as(u64, varnode.version);
    }
    
    /// Get or create a variable name
    fn getOrCreateVarName(self: *ExpressionReconstructor, var_id: u64) ![]const u8 {
        if (self.var_names.get(var_id)) |name| {
            return name;
        }
        
        // Generate a new name
        const name = try std.fmt.allocPrint(self.allocator, "var_{x}", .{var_id});
        try self.var_names.put(var_id, name);
        return name;
    }
};

/// Statement reconstructor
pub const StatementReconstructor = struct {
    allocator: std.mem.Allocator,
    expr_reconstructor: *ExpressionReconstructor,
    
    pub fn init(allocator: std.mem.Allocator, expr_reconstructor: *ExpressionReconstructor) StatementReconstructor {
        return .{
            .allocator = allocator,
            .expr_reconstructor = expr_reconstructor,
        };
    }
    
    /// Reconstruct a statement from an SSA instruction
    pub fn reconstructStatement(self: *StatementReconstructor, inst: *const SSAInstruction) !vb6_ast.Statement {
        switch (inst.opcode) {
            .copy, .int_add, .int_sub, .int_mult, .int_div => {
                // These are expressions, create assignment if there's an output
                if (inst.output) |output| {
                    const target = try self.allocator.create(Expression);
                    target.* = try self.expr_reconstructor.reconstructVarnode(output);
                    
                    const value = try self.expr_reconstructor.reconstructExpression(inst);
                    
                    return vb6_ast.Statement{
                        .assignment = vb6_ast.Assignment{
                            .target = target,
                            .value = value,
                        },
                    };
                }
            },
            
            .store => {
                // Store to memory - treat as assignment
                if (inst.input0 != null and inst.input1 != null) {
                    const target = try self.allocator.create(Expression);
                    target.* = try self.expr_reconstructor.reconstructVarnode(inst.input0.?);
                    
                    const value = try self.allocator.create(Expression);
                    value.* = try self.expr_reconstructor.reconstructVarnode(inst.input1.?);
                    
                    return vb6_ast.Statement{
                        .assignment = vb6_ast.Assignment{
                            .target = target,
                            .value = value,
                        },
                    };
                }
            },
            
            .call, .callind => {
                // Function/sub call
                const expr = try self.expr_reconstructor.reconstructExpression(inst);
                if (expr.* == .call) {
                    return vb6_ast.Statement{
                        .call_statement = vb6_ast.CallStatement{
                            .call = expr.call,
                        },
                    };
                }
            },
            
            .@"return" => {
                // Return statement
                if (inst.input0) |input| {
                    const return_expr = try self.allocator.create(Expression);
                    return_expr.* = try self.expr_reconstructor.reconstructVarnode(input);
                    return vb6_ast.Statement{ .return_value = return_expr };
                } else {
                    return vb6_ast.Statement{ .exit_sub = {} };
                }
            },
            
            else => {},
        }
        
        // Default: create a comment
        const comment = try std.fmt.allocPrint(self.allocator, "' {s}", .{@tagName(inst.opcode)});
        return vb6_ast.Statement{ .comment = comment };
    }
};
