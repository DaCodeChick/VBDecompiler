// VB6 P-code to Ghidra P-code Translator
// Translates VB6 stack-based bytecode to Ghidra P-code IR

const std = @import("std");
const vb_opcodes = @import("pcode_opcodes.zig");
const pcode = @import("../ir/pcode_ops.zig");
const pcode_func = @import("../ir/pcode_function.zig");

/// Translator for VB6 P-code to Ghidra P-code
pub const VBPCodeTranslator = struct {
    allocator: std.mem.Allocator,
    stack_ptr: u32,         // Virtual stack pointer
    temp_counter: u32,      // Temporary variable counter
    seq_num: u64,           // Sequence number for P-code ops
    
    const STACK_BASE: u32 = 0x10000;
    const VAR_BASE: u32 = 0x20000;
    
    pub fn init(allocator: std.mem.Allocator) VBPCodeTranslator {
        return .{
            .allocator = allocator,
            .stack_ptr = 0,
            .temp_counter = 0,
            .seq_num = 0,
        };
    }
    
    /// Translate VB6 P-code instructions to Ghidra P-code
    pub fn translate(self: *VBPCodeTranslator, vb_instructions: []const vb_opcodes.Instruction) !pcode_func.PCodeFunction {
        var function = pcode_func.PCodeFunction.init(self.allocator, 0, "vb_pcode_function");
        var current_block = pcode_func.PCodeBlock.init(self.allocator, 0);
        
        for (vb_instructions) |vb_instr| {
            var ops = try self.translateInstruction(&vb_instr);
            
            for (ops.items) |op| {
                try current_block.ops.append(self.allocator, op);
            }
            
            // Start new block on control flow instructions
            if (self.isControlFlow(vb_instr.opcode)) {
                try function.blocks.put(current_block.address, current_block);
                const next_addr = vb_instr.address + @as(u32, @intCast(vb_instr.getLength()));
                current_block = pcode_func.PCodeBlock.init(self.allocator, next_addr);
            }
            
            ops.deinit(self.allocator);
        }
        
        // Add final block
        if (current_block.ops.items.len > 0) {
            try function.blocks.put(current_block.address, current_block);
        }
        
        return function;
    }
    
    /// Translate single VB6 instruction to Ghidra P-code ops
    fn translateInstruction(self: *VBPCodeTranslator, instr: *const vb_opcodes.Instruction) !std.ArrayList(pcode.PCodeOp) {
        var ops: std.ArrayList(pcode.PCodeOp) = .empty;
        
        switch (instr.opcode) {
            // Stack operations
            .PushImm8, .PushImm16, .PushImm32 => try self.translatePushImm(&ops, instr.operand),
            .PushVar => try self.translatePushVar(&ops, instr.operand),
            .Pop => try self.translatePop(&ops),
            .Dup => try self.translateDup(&ops),
            
            // Arithmetic
            .Add => try self.translateBinaryOp(&ops, .int_add),
            .Sub => try self.translateBinaryOp(&ops, .int_sub),
            .Mul => try self.translateBinaryOp(&ops, .int_mult),
            .Div => try self.translateBinaryOp(&ops, .int_div),
            .Neg => try self.translateUnaryOp(&ops, .int_neg),
            
            // Logical
            .And => try self.translateBinaryOp(&ops, .int_and),
            .Or => try self.translateBinaryOp(&ops, .int_or),
            .Xor => try self.translateBinaryOp(&ops, .int_xor),
            .Not => try self.translateUnaryOp(&ops, .bool_negate),
            
            // Comparison
            .CmpEq => try self.translateBinaryOp(&ops, .int_equal),
            .CmpNe => try self.translateBinaryOp(&ops, .int_notequal),
            .CmpLt => try self.translateBinaryOp(&ops, .int_sless),
            .CmpLe => try self.translateBinaryOp(&ops, .int_slessequal),
            .CmpGt => try self.translateBinaryOpSwapped(&ops, .int_sless),  // a > b == b < a
            .CmpGe => try self.translateBinaryOpSwapped(&ops, .int_slessequal),  // a >= b == b <= a
            
            // Control flow
            .Jmp => try self.translateJump(&ops, instr.operand),
            .JmpT => try self.translateConditionalJump(&ops, instr.operand, true),
            .JmpF => try self.translateConditionalJump(&ops, instr.operand, false),
            .Call => try self.translateCall(&ops, instr.operand),
            .Ret => try self.translateReturn(&ops),
            
            // Assignment
            .Assign => try self.translateAssign(&ops, instr.operand),
            
            else => {
                // For unknown/unimplemented opcodes, add a NOP
                // In practice, we'd want to handle all opcodes
            },
        }
        
        return ops;
    }
    
    // Translation helpers for specific instruction types
    
    fn translatePushImm(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), value: u32) !void {
        const stack_loc = self.getStackLoc();
        const const_node = pcode.Varnode{
            .space = .constant,
            .offset = value,
            .size = 4,
        };
        const output = pcode.Varnode{
            .space = .stack,
            .offset = stack_loc,
            .size = 4,
        };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = .copy,
            .output = output,
            .input0 = const_node,
            .input1 = null,
            .seq_num = self.nextSeq(),
        });
        
        self.stack_ptr += 4;
    }
    
    fn translatePushVar(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), var_index: u32) !void {
        const var_loc = VAR_BASE + (var_index * 4);
        const stack_loc = self.getStackLoc();
        
        const input = pcode.Varnode{
            .space = .ram,
            .offset = var_loc,
            .size = 4,
        };
        const output = pcode.Varnode{
            .space = .stack,
            .offset = stack_loc,
            .size = 4,
        };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = .load,
            .output = output,
            .input0 = input,
            .input1 = null,
            .seq_num = self.nextSeq(),
        });
        
        self.stack_ptr += 4;
    }
    
    fn translatePop(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp)) !void {
        _ = ops;
        if (self.stack_ptr >= 4) {
            self.stack_ptr -= 4;
        }
    }
    
    fn translateDup(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp)) !void {
        if (self.stack_ptr < 4) return;
        
        const top_loc = self.stack_ptr - 4;
        const new_loc = self.getStackLoc();
        
        const input = pcode.Varnode{
            .space = .stack,
            .offset = STACK_BASE + top_loc,
            .size = 4,
        };
        const output = pcode.Varnode{
            .space = .stack,
            .offset = new_loc,
            .size = 4,
        };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = .copy,
            .output = output,
            .input0 = input,
            .input1 = null,
            .seq_num = self.nextSeq(),
        });
        
        self.stack_ptr += 4;
    }
    
    fn translateBinaryOp(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), op: pcode.OpCode) !void {
        if (self.stack_ptr < 8) return;
        
        // Pop two operands
        self.stack_ptr -= 4;
        const right_loc = STACK_BASE + self.stack_ptr;
        self.stack_ptr -= 4;
        const left_loc = STACK_BASE + self.stack_ptr;
        
        const result_loc = self.getStackLoc();
        
        const left = pcode.Varnode{ .space = .stack, .offset = left_loc, .size = 4 };
        const right = pcode.Varnode{ .space = .stack, .offset = right_loc, .size = 4 };
        const output = pcode.Varnode{ .space = .stack, .offset = result_loc, .size = 4 };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = op,
            .output = output,
            .input0 = left,
            .input1 = right,
            .seq_num = self.nextSeq(),
        });
        
        self.stack_ptr += 4;
    }
    
    fn translateBinaryOpSwapped(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), op: pcode.OpCode) !void {
        if (self.stack_ptr < 8) return;
        
        // Pop two operands and swap them
        self.stack_ptr -= 4;
        const right_loc = STACK_BASE + self.stack_ptr;
        self.stack_ptr -= 4;
        const left_loc = STACK_BASE + self.stack_ptr;
        
        const result_loc = self.getStackLoc();
        
        // Swap left and right for greater-than operations
        const left = pcode.Varnode{ .space = .stack, .offset = left_loc, .size = 4 };
        const right = pcode.Varnode{ .space = .stack, .offset = right_loc, .size = 4 };
        const output = pcode.Varnode{ .space = .stack, .offset = result_loc, .size = 4 };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = op,
            .output = output,
            .input0 = right,  // Swapped
            .input1 = left,   // Swapped
            .seq_num = self.nextSeq(),
        });
        
        self.stack_ptr += 4;
    }
    
    fn translateUnaryOp(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), op: pcode.OpCode) !void {
        if (self.stack_ptr < 4) return;
        
        self.stack_ptr -= 4;
        const operand_loc = STACK_BASE + self.stack_ptr;
        const result_loc = self.getStackLoc();
        
        const input = pcode.Varnode{ .space = .stack, .offset = operand_loc, .size = 4 };
        const output = pcode.Varnode{ .space = .stack, .offset = result_loc, .size = 4 };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = op,
            .output = output,
            .input0 = input,
            .input1 = null,
            .seq_num = self.nextSeq(),
        });
        
        self.stack_ptr += 4;
    }
    
    fn translateJump(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), target: u32) !void {
        const target_node = pcode.Varnode{
            .space = .constant,
            .offset = target,
            .size = 4,
        };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = .branch,
            .output = null,
            .input0 = target_node,
            .input1 = null,
            .seq_num = self.nextSeq(),
        });
    }
    
    fn translateConditionalJump(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), target: u32, if_true: bool) !void {
        if (self.stack_ptr < 4) return;
        
        self.stack_ptr -= 4;
        const cond_loc = STACK_BASE + self.stack_ptr;
        
        const condition = pcode.Varnode{ .space = .stack, .offset = cond_loc, .size = 4 };
        const target_node = pcode.Varnode{
            .space = .constant,
            .offset = target,
            .size = 4,
        };
        
        const op_code = if (if_true) pcode.OpCode.cbranch else pcode.OpCode.branch;
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = op_code,
            .output = null,
            .input0 = target_node,
            .input1 = condition,
            .seq_num = self.nextSeq(),
        });
    }
    
    fn translateCall(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), func_index: u32) !void {
        const target = pcode.Varnode{
            .space = .constant,
            .offset = func_index,
            .size = 4,
        };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = .call,
            .output = null,
            .input0 = target,
            .input1 = null,
            .seq_num = self.nextSeq(),
        });
    }
    
    fn translateReturn(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp)) !void {
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = .@"return",
            .output = null,
            .input0 = null,
            .input1 = null,
            .seq_num = self.nextSeq(),
        });
    }
    
    fn translateAssign(self: *VBPCodeTranslator, ops: *std.ArrayList(pcode.PCodeOp), var_index: u32) !void {
        if (self.stack_ptr < 4) return;
        
        self.stack_ptr -= 4;
        const value_loc = STACK_BASE + self.stack_ptr;
        const var_loc = VAR_BASE + (var_index * 4);
        
        const value = pcode.Varnode{ .space = .stack, .offset = value_loc, .size = 4 };
        const addr = pcode.Varnode{ .space = .constant, .offset = var_loc, .size = 4 };
        
        try ops.append(self.allocator, pcode.PCodeOp{
            .opcode = .store,
            .output = null,
            .input0 = addr,
            .input1 = value,
            .seq_num = self.nextSeq(),
        });
    }
    
    fn getStackLoc(self: *VBPCodeTranslator) u32 {
        return STACK_BASE + self.stack_ptr;
    }
    
    fn nextSeq(self: *VBPCodeTranslator) u64 {
        const seq = self.seq_num;
        self.seq_num += 1;
        return seq;
    }
    
    fn isControlFlow(self: *VBPCodeTranslator, opcode: vb_opcodes.Opcode) bool {
        _ = self;
        return switch (opcode) {
            .Jmp, .JmpT, .JmpF, .Call, .Ret => true,
            else => false,
        };
    }
};
