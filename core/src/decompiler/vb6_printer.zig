// VB6 Code Printer - Pretty-prints VB6 AST to readable VB6 code

const std = @import("std");
const vb6_ast = @import("vb6_ast.zig");
const Expression = vb6_ast.Expression;
const Statement = vb6_ast.Statement;
const Procedure = vb6_ast.Procedure;
const VBType = @import("../analysis/type_lattice.zig").VBType;

pub const VB6PrinterOptions = struct {
    indent_size: usize = 4,
    use_color: bool = false,
};

pub const VB6Printer = struct {
    allocator: std.mem.Allocator,
    options: VB6PrinterOptions,
    indent_level: usize,
    
    pub fn init(allocator: std.mem.Allocator, options: VB6PrinterOptions) VB6Printer {
        return .{
            .allocator = allocator,
            .options = options,
            .indent_level = 0,
        };
    }
    
    /// Print a complete procedure
    pub fn printProcedure(self: *VB6Printer, writer: anytype, proc: *const Procedure) !void {
        // Print procedure header
        switch (proc.kind) {
            .sub => {
                try writer.writeAll("Sub ");
                try writer.writeAll(proc.name);
            },
            .function => {
                try writer.writeAll("Function ");
                try writer.writeAll(proc.name);
            },
            .property_get => {
                try writer.writeAll("Property Get ");
                try writer.writeAll(proc.name);
            },
            .property_let => {
                try writer.writeAll("Property Let ");
                try writer.writeAll(proc.name);
            },
            .property_set => {
                try writer.writeAll("Property Set ");
                try writer.writeAll(proc.name);
            },
        }
        
        // Print parameters
        try writer.writeAll("(");
        for (proc.parameters.items, 0..) |param, i| {
            if (i > 0) try writer.writeAll(", ");
            
            if (param.by_ref) {
                try writer.writeAll("ByRef ");
            } else {
                try writer.writeAll("ByVal ");
            }
            
            try writer.writeAll(param.name);
            try writer.writeAll(" As ");
            try writer.writeAll(self.typeToString(param.vb_type));
        }
        try writer.writeAll(")");
        
        // Print return type for functions
        if (proc.return_type) |ret_type| {
            try writer.writeAll(" As ");
            try writer.writeAll(self.typeToString(ret_type));
        }
        
        try writer.writeAll("\n");
        
        self.indent_level += 1;
        
        // Print local variable declarations
        if (proc.local_variables.items.len > 0) {
            for (proc.local_variables.items) |var_decl| {
                try self.writeIndent(writer);
                try writer.writeAll("Dim ");
                try writer.writeAll(var_decl.name);
                try writer.writeAll(" As ");
                try writer.writeAll(self.typeToString(var_decl.vb_type));
                try writer.writeAll("\n");
            }
            try writer.writeAll("\n");
        }
        
        // Print body
        for (proc.body.items) |*stmt| {
            try self.printStatement(writer, stmt);
        }
        
        self.indent_level -= 1;
        
        // Print procedure footer
        switch (proc.kind) {
            .sub, .property_let, .property_set => try writer.writeAll("End Sub\n"),
            .function, .property_get => try writer.writeAll("End Function\n"),
        }
    }
    
    /// Print a statement
    pub fn printStatement(self: *VB6Printer, writer: anytype, stmt: *const Statement) !void {
        try self.writeIndent(writer);
        
        switch (stmt.*) {
            .dim => |dim| {
                try writer.writeAll("Dim ");
                try writer.writeAll(dim.variable.name);
                try writer.writeAll(" As ");
                try writer.writeAll(self.typeToString(dim.variable.vb_type));
                
                if (dim.initializer) |initializer| {
                    try writer.writeAll(" = ");
                    try self.printExpression(writer, initializer);
                }
                
                try writer.writeAll("\n");
            },
            
            .assignment => |assign| {
                try self.printExpression(writer, assign.target);
                try writer.writeAll(" = ");
                try self.printExpression(writer, assign.value);
                try writer.writeAll("\n");
            },
            
            .if_statement => |if_stmt| {
                try writer.writeAll("If ");
                try self.printExpression(writer, if_stmt.condition);
                try writer.writeAll(" Then\n");
                
                self.indent_level += 1;
                for (if_stmt.then_block.items) |*then_stmt| {
                    try self.printStatement(writer, then_stmt);
                }
                self.indent_level -= 1;
                
                // ElseIf blocks
                for (if_stmt.else_if_blocks.items) |*elif| {
                    try self.writeIndent(writer);
                    try writer.writeAll("ElseIf ");
                    try self.printExpression(writer, elif.condition);
                    try writer.writeAll(" Then\n");
                    
                    self.indent_level += 1;
                    for (elif.block.items) |*elif_stmt| {
                        try self.printStatement(writer, elif_stmt);
                    }
                    self.indent_level -= 1;
                }
                
                // Else block
                if (if_stmt.else_block) |*else_block| {
                    try self.writeIndent(writer);
                    try writer.writeAll("Else\n");
                    
                    self.indent_level += 1;
                    for (else_block.items) |*else_stmt| {
                        try self.printStatement(writer, else_stmt);
                    }
                    self.indent_level -= 1;
                }
                
                try self.writeIndent(writer);
                try writer.writeAll("End If\n");
            },
            
            .for_loop => |for_loop| {
                try writer.writeAll("For ");
                try writer.writeAll(for_loop.variable.name);
                try writer.writeAll(" = ");
                try self.printExpression(writer, for_loop.start);
                try writer.writeAll(" To ");
                try self.printExpression(writer, for_loop.end);
                
                if (for_loop.step) |step| {
                    try writer.writeAll(" Step ");
                    try self.printExpression(writer, step);
                }
                
                try writer.writeAll("\n");
                
                self.indent_level += 1;
                for (for_loop.body.items) |*body_stmt| {
                    try self.printStatement(writer, body_stmt);
                }
                self.indent_level -= 1;
                
                try self.writeIndent(writer);
                try writer.writeAll("Next ");
                try writer.writeAll(for_loop.variable.name);
                try writer.writeAll("\n");
            },
            
            .while_loop => |while_loop| {
                try writer.writeAll("While ");
                try self.printExpression(writer, while_loop.condition);
                try writer.writeAll("\n");
                
                self.indent_level += 1;
                for (while_loop.body.items) |*body_stmt| {
                    try self.printStatement(writer, body_stmt);
                }
                self.indent_level -= 1;
                
                try self.writeIndent(writer);
                try writer.writeAll("Wend\n");
            },
            
            .call_statement => |call_stmt| {
                try writer.writeAll(call_stmt.call.target);
                if (call_stmt.call.arguments.items.len > 0) {
                    try writer.writeAll(" ");
                    for (call_stmt.call.arguments.items, 0..) |arg, i| {
                        if (i > 0) try writer.writeAll(", ");
                        try self.printExpression(writer, arg);
                    }
                }
                try writer.writeAll("\n");
            },
            
            .exit_sub => {
                try writer.writeAll("Exit Sub\n");
            },
            
            .exit_function => {
                try writer.writeAll("Exit Function\n");
            },
            
            .return_value => |expr| {
                // In VB6, return is implicit via function name assignment
                try writer.writeAll("' Return ");
                try self.printExpression(writer, expr);
                try writer.writeAll("\n");
            },
            
            .comment => |comment| {
                try writer.writeAll(comment);
                try writer.writeAll("\n");
            },
            
            else => {
                try writer.writeAll("' TODO: Implement statement type\n");
            },
        }
    }
    
    /// Print an expression
    pub fn printExpression(self: *VB6Printer, writer: anytype, expr: *const Expression) !void {
        switch (expr.*) {
            .integer_literal => |val| {
                try writer.print("{d}", .{val});
            },
            
            .long_literal => |val| {
                try writer.print("{d}&", .{val});
            },
            
            .float_literal => |val| {
                try writer.print("{d}", .{val});
            },
            
            .string_literal => |str| {
                try writer.print("\"{s}\"", .{str});
            },
            
            .boolean_literal => |val| {
                try writer.writeAll(if (val) "True" else "False");
            },
            
            .variable => |var_ref| {
                try writer.writeAll(var_ref.name);
            },
            
            .binary_op => |binop| {
                try writer.writeAll("(");
                try self.printExpression(writer, binop.left);
                try writer.writeAll(" ");
                try writer.writeAll(binop.op.toVB6String());
                try writer.writeAll(" ");
                try self.printExpression(writer, binop.right);
                try writer.writeAll(")");
            },
            
            .unary_op => |unop| {
                try writer.writeAll(unop.op.toVB6String());
                try writer.writeAll("(");
                try self.printExpression(writer, unop.operand);
                try writer.writeAll(")");
            },
            
            .call => |call| {
                try writer.writeAll(call.target);
                try writer.writeAll("(");
                for (call.arguments.items, 0..) |arg, i| {
                    if (i > 0) try writer.writeAll(", ");
                    try self.printExpression(writer, arg);
                }
                try writer.writeAll(")");
            },
            
            .member_access => |member| {
                try self.printExpression(writer, member.object);
                try writer.writeAll(".");
                try writer.writeAll(member.member);
            },
            
            .array_access => |array| {
                try self.printExpression(writer, array.array);
                try writer.writeAll("(");
                for (array.indices.items, 0..) |idx, i| {
                    if (i > 0) try writer.writeAll(", ");
                    try self.printExpression(writer, idx);
                }
                try writer.writeAll(")");
            },
            
            else => {
                try writer.writeAll("<?>");
            },
        }
    }
    
    /// Write indentation
    fn writeIndent(self: *VB6Printer, writer: anytype) !void {
        const spaces = self.indent_level * self.options.indent_size;
        var i: usize = 0;
        while (i < spaces) : (i += 1) {
            try writer.writeAll(" ");
        }
    }
    
    /// Convert VB type to string
    fn typeToString(self: *VB6Printer, vb_type: VBType) []const u8 {
        _ = self;
        return switch (vb_type) {
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
            .udt => "Type",
            else => "Variant",
        };
    }
};
