// VB6 Abstract Syntax Tree (AST) for decompiler output
// Represents high-level VB6 code structures

const std = @import("std");
const VBType = @import("../analysis/type_lattice.zig").VBType;

/// VB6 expression types
pub const Expression = union(enum) {
    // Literals
    integer_literal: i32,
    long_literal: i64,
    float_literal: f64,
    string_literal: []const u8,
    boolean_literal: bool,
    
    // Variables and identifiers
    variable: Variable,
    
    // Operators
    binary_op: BinaryOp,
    unary_op: UnaryOp,
    
    // Function/method calls
    call: Call,
    
    // Member access (object.property)
    member_access: MemberAccess,
    
    // Array access
    array_access: ArrayAccess,
    
    // Type conversion
    type_cast: TypeCast,
    
    pub fn deinit(self: *Expression, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .string_literal => |s| allocator.free(s),
            .binary_op => |*op| op.deinit(allocator),
            .unary_op => |*op| op.deinit(allocator),
            .call => |*c| c.deinit(allocator),
            .member_access => |*m| m.deinit(allocator),
            .array_access => |*a| a.deinit(allocator),
            .type_cast => |*t| t.deinit(allocator),
            else => {},
        }
    }
};

pub const Variable = struct {
    name: []const u8,
    vb_type: VBType,
    
    pub fn init(name: []const u8, vb_type: VBType) Variable {
        return .{ .name = name, .vb_type = vb_type };
    }
};

pub const BinaryOp = struct {
    op: BinaryOpKind,
    left: *Expression,
    right: *Expression,
    
    pub fn deinit(self: *BinaryOp, allocator: std.mem.Allocator) void {
        self.left.deinit(allocator);
        allocator.destroy(self.left);
        self.right.deinit(allocator);
        allocator.destroy(self.right);
    }
};

pub const BinaryOpKind = enum {
    // Arithmetic
    add,      // +
    sub,      // -
    mul,      // *
    div,      // /
    int_div,  // \
    mod_op,   // Mod
    
    // Comparison
    eq,       // =
    ne,       // <>
    lt,       // <
    le,       // <=
    gt,       // >
    ge,       // >=
    
    // Logical
    and_op,   // And
    or_op,    // Or
    xor,      // Xor
    
    // String
    concat,   // &
    
    pub fn toVB6String(self: BinaryOpKind) []const u8 {
        return switch (self) {
            .add => "+",
            .sub => "-",
            .mul => "*",
            .div => "/",
            .int_div => "\\",
            .mod_op => "Mod",
            .eq => "=",
            .ne => "<>",
            .lt => "<",
            .le => "<=",
            .gt => ">",
            .ge => ">=",
            .and_op => "And",
            .or_op => "Or",
            .xor => "Xor",
            .concat => "&",
        };
    }
};

pub const UnaryOp = struct {
    op: UnaryOpKind,
    operand: *Expression,
    
    pub fn deinit(self: *UnaryOp, allocator: std.mem.Allocator) void {
        self.operand.deinit(allocator);
        allocator.destroy(self.operand);
    }
};

pub const UnaryOpKind = enum {
    negate,  // -
    not_op,  // Not
    
    pub fn toVB6String(self: UnaryOpKind) []const u8 {
        return switch (self) {
            .negate => "-",
            .not_op => "Not",
        };
    }
};

pub const Call = struct {
    target: []const u8,
    arguments: std.ArrayList(*Expression),
    
    pub fn deinit(self: *Call, allocator: std.mem.Allocator) void {
        for (self.arguments.items) |arg| {
            arg.deinit(allocator);
            allocator.destroy(arg);
        }
        self.arguments.deinit(allocator);
    }
};

pub const MemberAccess = struct {
    object: *Expression,
    member: []const u8,
    
    pub fn deinit(self: *MemberAccess, allocator: std.mem.Allocator) void {
        self.object.deinit(allocator);
        allocator.destroy(self.object);
    }
};

pub const ArrayAccess = struct {
    array: *Expression,
    indices: std.ArrayList(*Expression),
    
    pub fn deinit(self: *ArrayAccess, allocator: std.mem.Allocator) void {
        self.array.deinit(allocator);
        allocator.destroy(self.array);
        for (self.indices.items) |idx| {
            idx.deinit(allocator);
            allocator.destroy(idx);
        }
        self.indices.deinit(allocator);
    }
};

pub const TypeCast = struct {
    expression: *Expression,
    target_type: VBType,
    
    pub fn deinit(self: *TypeCast, allocator: std.mem.Allocator) void {
        self.expression.deinit(allocator);
        allocator.destroy(self.expression);
    }
};

/// VB6 statement types
pub const Statement = union(enum) {
    // Variable declaration
    dim: DimStatement,
    
    // Assignment
    assignment: Assignment,
    
    // Control flow
    if_statement: IfStatement,
    select_case: SelectCase,
    for_loop: ForLoop,
    do_loop: DoLoop,
    while_loop: WhileLoop,
    
    // Calls
    call_statement: CallStatement,
    
    // Return/Exit
    exit_sub: void,
    exit_function: void,
    return_value: *Expression,
    
    // Error handling
    on_error: OnError,
    
    // Comments
    comment: []const u8,
    
    pub fn deinit(self: *Statement, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .dim => |*d| d.deinit(allocator),
            .assignment => |*a| a.deinit(allocator),
            .if_statement => |*i| i.deinit(allocator),
            .select_case => |*s| s.deinit(allocator),
            .for_loop => |*f| f.deinit(allocator),
            .do_loop => |*d| d.deinit(allocator),
            .while_loop => |*w| w.deinit(allocator),
            .call_statement => |*c| c.deinit(allocator),
            .return_value => |expr| {
                expr.deinit(allocator);
                allocator.destroy(expr);
            },
            .comment => |s| allocator.free(s),
            else => {},
        }
    }
};

pub const DimStatement = struct {
    variable: Variable,
    initializer: ?*Expression,
    
    pub fn deinit(self: *DimStatement, allocator: std.mem.Allocator) void {
        if (self.initializer) |init| {
            init.deinit(allocator);
            allocator.destroy(init);
        }
    }
};

pub const Assignment = struct {
    target: *Expression,
    value: *Expression,
    
    pub fn deinit(self: *Assignment, allocator: std.mem.Allocator) void {
        self.target.deinit(allocator);
        allocator.destroy(self.target);
        self.value.deinit(allocator);
        allocator.destroy(self.value);
    }
};

pub const IfStatement = struct {
    condition: *Expression,
    then_block: std.ArrayList(Statement),
    else_if_blocks: std.ArrayList(ElseIf),
    else_block: ?std.ArrayList(Statement),
    
    pub fn deinit(self: *IfStatement, allocator: std.mem.Allocator) void {
        self.condition.deinit(allocator);
        allocator.destroy(self.condition);
        
        for (self.then_block.items) |*stmt| {
            stmt.deinit(allocator);
        }
        self.then_block.deinit(allocator);
        
        for (self.else_if_blocks.items) |*elif| {
            elif.deinit(allocator);
        }
        self.else_if_blocks.deinit(allocator);
        
        if (self.else_block) |*eb| {
            for (eb.items) |*stmt| {
                stmt.deinit(allocator);
            }
            eb.deinit(allocator);
        }
    }
};

pub const ElseIf = struct {
    condition: *Expression,
    block: std.ArrayList(Statement),
    
    pub fn deinit(self: *ElseIf, allocator: std.mem.Allocator) void {
        self.condition.deinit(allocator);
        allocator.destroy(self.condition);
        for (self.block.items) |*stmt| {
            stmt.deinit(allocator);
        }
        self.block.deinit(allocator);
    }
};

pub const SelectCase = struct {
    expression: *Expression,
    cases: std.ArrayList(CaseClause),
    else_block: ?std.ArrayList(Statement),
    
    pub fn deinit(self: *SelectCase, allocator: std.mem.Allocator) void {
        self.expression.deinit(allocator);
        allocator.destroy(self.expression);
        
        for (self.cases.items) |*case| {
            case.deinit(allocator);
        }
        self.cases.deinit(allocator);
        
        if (self.else_block) |*eb| {
            for (eb.items) |*stmt| {
                stmt.deinit(allocator);
            }
            eb.deinit(allocator);
        }
    }
};

pub const CaseClause = struct {
    values: std.ArrayList(*Expression),
    block: std.ArrayList(Statement),
    
    pub fn deinit(self: *CaseClause, allocator: std.mem.Allocator) void {
        for (self.values.items) |val| {
            val.deinit(allocator);
            allocator.destroy(val);
        }
        self.values.deinit(allocator);
        
        for (self.block.items) |*stmt| {
            stmt.deinit(allocator);
        }
        self.block.deinit(allocator);
    }
};

pub const ForLoop = struct {
    variable: Variable,
    start: *Expression,
    end: *Expression,
    step: ?*Expression,
    body: std.ArrayList(Statement),
    
    pub fn deinit(self: *ForLoop, allocator: std.mem.Allocator) void {
        self.start.deinit(allocator);
        allocator.destroy(self.start);
        self.end.deinit(allocator);
        allocator.destroy(self.end);
        if (self.step) |step| {
            step.deinit(allocator);
            allocator.destroy(step);
        }
        for (self.body.items) |*stmt| {
            stmt.deinit(allocator);
        }
        self.body.deinit(allocator);
    }
};

pub const DoLoop = struct {
    kind: DoLoopKind,
    condition: ?*Expression,
    body: std.ArrayList(Statement),
    
    pub fn deinit(self: *DoLoop, allocator: std.mem.Allocator) void {
        if (self.condition) |cond| {
            cond.deinit(allocator);
            allocator.destroy(cond);
        }
        for (self.body.items) |*stmt| {
            stmt.deinit(allocator);
        }
        self.body.deinit(allocator);
    }
};

pub const DoLoopKind = enum {
    do_while,      // Do While ... Loop
    do_until,      // Do Until ... Loop
    loop_while,    // Do ... Loop While
    loop_until,    // Do ... Loop Until
    infinite,      // Do ... Loop
};

pub const WhileLoop = struct {
    condition: *Expression,
    body: std.ArrayList(Statement),
    
    pub fn deinit(self: *WhileLoop, allocator: std.mem.Allocator) void {
        self.condition.deinit(allocator);
        allocator.destroy(self.condition);
        for (self.body.items) |*stmt| {
            stmt.deinit(allocator);
        }
        self.body.deinit(allocator);
    }
};

pub const CallStatement = struct {
    call: Call,
    
    pub fn deinit(self: *CallStatement, allocator: std.mem.Allocator) void {
        self.call.deinit(allocator);
    }
};

pub const OnError = enum {
    goto_zero,        // On Error GoTo 0
    resume_next,      // On Error Resume Next
    goto_label,       // On Error GoTo <label>
};

/// VB6 procedure (Sub or Function)
pub const Procedure = struct {
    name: []const u8,
    kind: ProcedureKind,
    parameters: std.ArrayList(Parameter),
    return_type: ?VBType,
    local_variables: std.ArrayList(Variable),
    body: std.ArrayList(Statement),
    address: u32,
    allocator: std.mem.Allocator,
    
    pub fn init(allocator: std.mem.Allocator, name: []const u8, kind: ProcedureKind, address: u32) Procedure {
        return .{
            .name = name,
            .kind = kind,
            .parameters = std.ArrayList(Parameter).empty,
            .return_type = null,
            .local_variables = std.ArrayList(Variable).empty,
            .body = std.ArrayList(Statement).empty,
            .address = address,
            .allocator = allocator,
        };
    }
    
    pub fn deinit(self: *Procedure) void {
        self.parameters.deinit(self.allocator);
        self.local_variables.deinit(self.allocator);
        for (self.body.items) |*stmt| {
            stmt.deinit(self.allocator);
        }
        self.body.deinit(self.allocator);
    }
};

pub const ProcedureKind = enum {
    sub,
    function,
    property_get,
    property_let,
    property_set,
};

pub const Parameter = struct {
    name: []const u8,
    vb_type: VBType,
    by_ref: bool,
    optional: bool,
    default_value: ?*Expression,
};
