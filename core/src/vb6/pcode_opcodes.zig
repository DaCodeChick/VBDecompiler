// VB6 P-code Opcodes
// VB6 uses a stack-based bytecode format when compiled with "Compile to P-Code" option
// This module defines the opcodes and instruction structures

const std = @import("std");

/// VB6 P-code opcode enumeration
/// Based on reverse engineering of MSVBVM60.DLL
pub const Opcode = enum(u8) {
    // Stack operations
    PushImm8 = 0x01,    // Push 8-bit immediate
    PushImm16 = 0x02,   // Push 16-bit immediate
    PushImm32 = 0x03,   // Push 32-bit immediate
    PushVar = 0x04,     // Push variable value
    PushVarRef = 0x05,  // Push variable reference
    Pop = 0x06,         // Pop from stack
    Dup = 0x07,         // Duplicate top of stack
    
    // Arithmetic operations
    Add = 0x10,         // Addition
    Sub = 0x11,         // Subtraction
    Mul = 0x12,         // Multiplication
    Div = 0x13,         // Division
    Mod = 0x14,         // Modulo
    Neg = 0x15,         // Negation
    
    // Logical operations
    And = 0x20,         // Logical AND
    Or = 0x21,          // Logical OR
    Not = 0x22,         // Logical NOT
    Xor = 0x23,         // Logical XOR
    
    // Comparison operations
    CmpEq = 0x30,       // Compare equal
    CmpNe = 0x31,       // Compare not equal
    CmpLt = 0x32,       // Compare less than
    CmpLe = 0x33,       // Compare less or equal
    CmpGt = 0x34,       // Compare greater than
    CmpGe = 0x35,       // Compare greater or equal
    
    // Control flow
    Jmp = 0x40,         // Unconditional jump
    JmpT = 0x41,        // Jump if true
    JmpF = 0x42,        // Jump if false
    Call = 0x43,        // Call function
    Ret = 0x44,         // Return from function
    CallNative = 0x45,  // Call native code
    
    // Object operations
    GetProp = 0x50,     // Get object property
    SetProp = 0x51,     // Set object property
    CallMethod = 0x52,  // Call object method
    CreateObj = 0x53,   // Create object
    
    // Array operations
    GetElem = 0x60,     // Get array element
    SetElem = 0x61,     // Set array element
    Redim = 0x62,       // Redim array
    
    // String operations
    StrConcat = 0x70,   // String concatenation
    StrCmp = 0x71,      // String comparison
    StrLen = 0x72,      // String length
    
    // Type conversion
    CvtInt = 0x80,      // Convert to Integer
    CvtLong = 0x81,     // Convert to Long
    CvtSingle = 0x82,   // Convert to Single
    CvtDouble = 0x83,   // Convert to Double
    CvtString = 0x84,   // Convert to String
    CvtVariant = 0x85,  // Convert to Variant
    
    // Variable assignment
    Assign = 0x90,      // Assignment
    AssignRef = 0x91,   // Assignment by reference
    
    // Special
    Nop = 0xFF,         // No operation
    
    _,                  // Allow unknown opcodes
};

/// P-code instruction operand types
pub const OperandType = enum {
    None,
    Imm8,       // 8-bit immediate
    Imm16,      // 16-bit immediate
    Imm32,      // 32-bit immediate
    VarIndex,   // Variable index (16-bit)
    Offset,     // Jump offset (16-bit signed)
    FuncIndex,  // Function index (16-bit)
    PropIndex,  // Property index (16-bit)
};

/// P-code instruction structure
pub const Instruction = struct {
    opcode: Opcode,
    address: u32,           // Address in P-code segment
    operand_type: OperandType,
    operand: u32,           // Generic operand value
    
    /// Get instruction length in bytes
    pub fn getLength(self: *const Instruction) usize {
        const base: usize = 1;
        const operand_size: usize = switch (self.operand_type) {
            .None => 0,
            .Imm8, .VarIndex => 1,
            .Imm16, .Offset, .FuncIndex, .PropIndex => 2,
            .Imm32 => 4,
        };
        return base + operand_size;
    }
    
    /// Format instruction as string
    pub fn format(self: *const Instruction, allocator: std.mem.Allocator) ![]const u8 {
        const opcode_name = getOpcodeName(self.opcode);
        
        return switch (self.operand_type) {
            .None => try std.fmt.allocPrint(allocator, "{s}", .{opcode_name}),
            .Imm8, .Imm16, .Imm32 => try std.fmt.allocPrint(allocator, "{s} #{d}", .{ opcode_name, self.operand }),
            .VarIndex => try std.fmt.allocPrint(allocator, "{s} var_{d}", .{ opcode_name, self.operand }),
            .Offset => try std.fmt.allocPrint(allocator, "{s} 0x{X:0>8}", .{ opcode_name, @as(i32, @bitCast(self.operand)) }),
            .FuncIndex => try std.fmt.allocPrint(allocator, "{s} func_{d}", .{ opcode_name, self.operand }),
            .PropIndex => try std.fmt.allocPrint(allocator, "{s} prop_{d}", .{ opcode_name, self.operand }),
        };
    }
};

/// Get human-readable name for opcode
pub fn getOpcodeName(opcode: Opcode) []const u8 {
    return switch (opcode) {
        .PushImm8, .PushImm16, .PushImm32 => "push",
        .PushVar => "pushvar",
        .PushVarRef => "pushref",
        .Pop => "pop",
        .Dup => "dup",
        .Add => "add",
        .Sub => "sub",
        .Mul => "mul",
        .Div => "div",
        .Mod => "mod",
        .Neg => "neg",
        .And => "and",
        .Or => "or",
        .Not => "not",
        .Xor => "xor",
        .CmpEq => "cmpeq",
        .CmpNe => "cmpne",
        .CmpLt => "cmplt",
        .CmpLe => "cmple",
        .CmpGt => "cmpgt",
        .CmpGe => "cmpge",
        .Jmp => "jmp",
        .JmpT => "jt",
        .JmpF => "jf",
        .Call => "call",
        .Ret => "ret",
        .CallNative => "callnative",
        .GetProp => "getprop",
        .SetProp => "setprop",
        .CallMethod => "callmethod",
        .CreateObj => "createobj",
        .GetElem => "getelem",
        .SetElem => "setelem",
        .Redim => "redim",
        .StrConcat => "strconcat",
        .StrCmp => "strcmp",
        .StrLen => "strlen",
        .CvtInt => "cvtint",
        .CvtLong => "cvtlong",
        .CvtSingle => "cvtsingle",
        .CvtDouble => "cvtdouble",
        .CvtString => "cvtstring",
        .CvtVariant => "cvtvariant",
        .Assign => "assign",
        .AssignRef => "assignref",
        .Nop => "nop",
        _ => "unknown",
    };
}

/// Get operand type for opcode
pub fn getOperandType(opcode: Opcode) OperandType {
    return switch (opcode) {
        .PushImm8 => .Imm8,
        .PushImm16 => .Imm16,
        .PushImm32 => .Imm32,
        .PushVar, .PushVarRef => .VarIndex,
        .Jmp, .JmpT, .JmpF => .Offset,
        .Call => .FuncIndex,
        .GetProp, .SetProp => .PropIndex,
        .CallMethod => .FuncIndex,
        else => .None,
    };
}
