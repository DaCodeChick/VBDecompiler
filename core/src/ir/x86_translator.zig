// x86 to P-code translator
// Translates x86 instructions to Ghidra P-code intermediate representation

const std = @import("std");
const pcode = @import("pcode_ops.zig");
const inst = @import("../disasm/instruction.zig");

const OpCode = pcode.OpCode;
const Varnode = pcode.Varnode;
const PCodeOp = pcode.PCodeOp;
const AddressSpace = pcode.AddressSpace;
const Instruction = inst.Instruction;
const Mnemonic = inst.Mnemonic;
const Register = inst.Register;
const Operand = inst.Operand;

/// x86 register to P-code register offset mapping
pub const X86RegisterMap = struct {
    /// Get the base register offset for a given x86 register
    /// We map x86 registers to unique offsets in the register address space
    pub fn getRegisterOffset(reg: Register) u64 {
        return switch (reg) {
            // 32-bit registers (canonical form)
            .eax => 0,
            .ecx => 1,
            .edx => 2,
            .ebx => 3,
            .esp => 4,
            .ebp => 5,
            .esi => 6,
            .edi => 7,

            // 16-bit registers map to low 2 bytes of 32-bit regs
            .ax => 0,
            .cx => 1,
            .dx => 2,
            .bx => 3,
            .sp => 4,
            .bp => 5,
            .si => 6,
            .di => 7,

            // 8-bit low registers (al, cl, dl, bl)
            .al => 0,
            .cl => 1,
            .dl => 2,
            .bl => 3,

            // 8-bit high registers (ah, ch, dh, bh) - offset by 0x100
            .ah => 0x100,
            .ch => 0x101,
            .dh => 0x102,
            .bh => 0x103,

            // Segment registers
            .es => 0x200,
            .cs => 0x201,
            .ss => 0x202,
            .ds => 0x203,
            .fs => 0x204,
            .gs => 0x205,

            .none => 0xFFFF,
        };
    }

    /// Convert x86 register to P-code varnode
    pub fn toVarnode(reg: Register) Varnode {
        const offset = getRegisterOffset(reg);
        const size = reg.getSize();
        return Varnode.register(offset, size);
    }
};

/// P-code translator context
pub const Translator = struct {
    allocator: std.mem.Allocator,
    ops: std.ArrayList(PCodeOp),
    next_unique: u64, // Counter for unique temporary variables
    next_seq: u64, // Sequence number within current instruction

    pub fn init(allocator: std.mem.Allocator) Translator {
        return .{
            .allocator = allocator,
            .ops = .{ .items = &.{}, .capacity = 0 },
            .next_unique = 0,
            .next_seq = 0,
        };
    }

    pub fn deinit(self: *Translator) void {
        self.ops.deinit(self.allocator);
    }

    /// Allocate a new unique temporary varnode
    fn allocTemp(self: *Translator, size: u32) Varnode {
        const result = Varnode.unique(self.next_unique, size);
        self.next_unique += 1;
        return result;
    }

    /// Emit a P-code operation
    fn emit(self: *Translator, opcode: OpCode, output: ?Varnode, input0: ?Varnode, input1: ?Varnode) !void {
        try self.ops.append(self.allocator, .{
            .opcode = opcode,
            .output = output,
            .input0 = input0,
            .input1 = input1,
            .seq_num = self.next_seq,
        });
        self.next_seq += 1;
    }

    /// Translate x86 operand to P-code varnode
    /// For memory operands, this returns the *address* computation result
    fn translateOperandToAddress(self: *Translator, operand: *const Operand, size: u32) !Varnode {
        return switch (operand.*) {
            .register => |reg| X86RegisterMap.toVarnode(reg),
            .immediate => |imm| Varnode.constant(imm, size),
            .relative => |rel| Varnode.constant(@as(u32, @bitCast(rel)), size),
            .memory => |mem| blk: {
                // Compute effective address: base + index*scale + displacement
                var addr = self.allocTemp(4); // 32-bit address

                if (mem.base != .none and mem.index != .none) {
                    // base + index*scale + disp
                    const base_vn = X86RegisterMap.toVarnode(mem.base);
                    const index_vn = X86RegisterMap.toVarnode(mem.index);

                    // Calculate index * scale
                    var scaled_index = index_vn;
                    if (mem.scale > 1) {
                        const scale_const = Varnode.constant(mem.scale, 4);
                        scaled_index = self.allocTemp(4);
                        try self.emit(.int_mult, scaled_index, index_vn, scale_const);
                    }

                    // Add base + scaled_index
                    const temp = self.allocTemp(4);
                    try self.emit(.int_add, temp, base_vn, scaled_index);

                    // Add displacement if present
                    if (mem.has_displacement) {
                        const disp_const = Varnode.constant(@as(u32, @bitCast(mem.displacement)), 4);
                        try self.emit(.int_add, addr, temp, disp_const);
                    } else {
                        addr = temp;
                    }
                } else if (mem.base != .none) {
                    // base + disp
                    const base_vn = X86RegisterMap.toVarnode(mem.base);
                    if (mem.has_displacement) {
                        const disp_const = Varnode.constant(@as(u32, @bitCast(mem.displacement)), 4);
                        try self.emit(.int_add, addr, base_vn, disp_const);
                    } else {
                        addr = base_vn;
                    }
                } else if (mem.index != .none) {
                    // index*scale + disp
                    const index_vn = X86RegisterMap.toVarnode(mem.index);

                    var scaled_index = index_vn;
                    if (mem.scale > 1) {
                        const scale_const = Varnode.constant(mem.scale, 4);
                        scaled_index = self.allocTemp(4);
                        try self.emit(.int_mult, scaled_index, index_vn, scale_const);
                    }

                    if (mem.has_displacement) {
                        const disp_const = Varnode.constant(@as(u32, @bitCast(mem.displacement)), 4);
                        try self.emit(.int_add, addr, scaled_index, disp_const);
                    } else {
                        addr = scaled_index;
                    }
                } else {
                    // Just displacement (absolute address)
                    addr = Varnode.constant(@as(u32, @bitCast(mem.displacement)), 4);
                }

                break :blk addr;
            },
            .none => Varnode.constant(0, size),
        };
    }

    /// Load from memory if needed
    fn loadOperand(self: *Translator, operand: *const Operand, size: u32) !Varnode {
        return switch (operand.*) {
            .memory => blk: {
                const addr = try self.translateOperandToAddress(operand, size);
                const result = self.allocTemp(size);
                try self.emit(.load, result, addr, null);
                break :blk result;
            },
            else => try self.translateOperandToAddress(operand, size),
        };
    }

    /// Store to memory if needed
    fn storeOperand(self: *Translator, operand: *const Operand, value: Varnode) !void {
        switch (operand.*) {
            .memory => {
                const addr = try self.translateOperandToAddress(operand, value.size);
                try self.emit(.store, null, addr, value);
            },
            .register => |reg| {
                const dest = X86RegisterMap.toVarnode(reg);
                try self.emit(.copy, dest, value, null);
            },
            else => {}, // Cannot store to immediate/relative
        }
    }

    /// Translate a single x86 instruction to P-code
    pub fn translateInstruction(self: *Translator, instruction: *const Instruction) !void {
        self.next_seq = 0; // Reset sequence number for new instruction

        switch (instruction.mnemonic) {
            .mov => try self.translateMov(instruction),
            .push => try self.translatePush(instruction),
            .pop => try self.translatePop(instruction),
            .lea => try self.translateLea(instruction),
            
            .add => try self.translateBinaryOp(instruction, .int_add),
            .sub => try self.translateBinaryOp(instruction, .int_sub),
            .and_ => try self.translateBinaryOp(instruction, .int_and),
            .or_ => try self.translateBinaryOp(instruction, .int_or),
            .xor => try self.translateBinaryOp(instruction, .int_xor),
            
            .inc => try self.translateIncDec(instruction, .int_add),
            .dec => try self.translateIncDec(instruction, .int_sub),
            
            .neg => try self.translateNeg(instruction),
            .not => try self.translateNot(instruction),
            
            .cmp => try self.translateCmp(instruction),
            .test_ => try self.translateTest(instruction),
            
            .jmp => try self.translateJmp(instruction),
            .je, .jz => try self.translateConditionalJmp(instruction, .int_equal),
            .jne, .jnz => try self.translateConditionalJmp(instruction, .int_notequal),
            .jl => try self.translateConditionalJmp(instruction, .int_sless),
            .jle => try self.translateConditionalJmp(instruction, .int_slessequal),
            .jg => try self.translateConditionalJmpInverted(instruction, .int_slessequal),
            .jge => try self.translateConditionalJmpInverted(instruction, .int_sless),
            .jb => try self.translateConditionalJmp(instruction, .int_less),
            .jbe => try self.translateConditionalJmp(instruction, .int_lessequal),
            .ja => try self.translateConditionalJmpInverted(instruction, .int_lessequal),
            .jae => try self.translateConditionalJmpInverted(instruction, .int_less),
            
            .call => try self.translateCall(instruction),
            .ret, .retn => try self.translateRet(instruction),
            
            .nop => {}, // NOP produces no P-code
            
            else => {
                // For unsupported instructions, emit a comment or placeholder
                // In a full implementation, we'd handle all x86 instructions
                std.debug.print("Warning: Unsupported instruction: {s}\n", .{instruction.mnemonic.getName()});
            },
        }
    }

    fn translateMov(self: *Translator, instruction: *const Instruction) !void {
        const src = try self.loadOperand(&instruction.operands[1], 4);
        try self.storeOperand(&instruction.operands[0], src);
    }

    fn translatePush(self: *Translator, instruction: *const Instruction) !void {
        const esp = X86RegisterMap.toVarnode(.esp);
        const src = try self.loadOperand(&instruction.operands[0], 4);
        
        // ESP = ESP - 4
        const four = Varnode.constant(4, 4);
        const new_esp = self.allocTemp(4);
        try self.emit(.int_sub, new_esp, esp, four);
        try self.emit(.copy, esp, new_esp, null);
        
        // [ESP] = src
        try self.emit(.store, null, new_esp, src);
    }

    fn translatePop(self: *Translator, instruction: *const Instruction) !void {
        const esp = X86RegisterMap.toVarnode(.esp);
        
        // value = [ESP]
        const value = self.allocTemp(4);
        try self.emit(.load, value, esp, null);
        try self.storeOperand(&instruction.operands[0], value);
        
        // ESP = ESP + 4
        const four = Varnode.constant(4, 4);
        const new_esp = self.allocTemp(4);
        try self.emit(.int_add, new_esp, esp, four);
        try self.emit(.copy, esp, new_esp, null);
    }

    fn translateLea(self: *Translator, instruction: *const Instruction) !void {
        // LEA loads the effective address (not the value at that address)
        const addr = try self.translateOperandToAddress(&instruction.operands[1], 4);
        try self.storeOperand(&instruction.operands[0], addr);
    }

    fn translateBinaryOp(self: *Translator, instruction: *const Instruction, opcode: OpCode) !void {
        const dest_size: u32 = 4; // TODO: infer from operand
        const src1 = try self.loadOperand(&instruction.operands[0], dest_size);
        const src2 = try self.loadOperand(&instruction.operands[1], dest_size);
        const result = self.allocTemp(dest_size);
        
        try self.emit(opcode, result, src1, src2);
        try self.storeOperand(&instruction.operands[0], result);
        
        // TODO: Set flags (ZF, SF, CF, OF, etc.)
    }

    fn translateIncDec(self: *Translator, instruction: *const Instruction, opcode: OpCode) !void {
        const dest_size: u32 = 4;
        const src = try self.loadOperand(&instruction.operands[0], dest_size);
        const one = Varnode.constant(1, dest_size);
        const result = self.allocTemp(dest_size);
        
        try self.emit(opcode, result, src, one);
        try self.storeOperand(&instruction.operands[0], result);
        
        // TODO: Set flags
    }

    fn translateNeg(self: *Translator, instruction: *const Instruction) !void {
        const dest_size: u32 = 4;
        const src = try self.loadOperand(&instruction.operands[0], dest_size);
        const result = self.allocTemp(dest_size);
        
        try self.emit(.int_neg, result, src, null);
        try self.storeOperand(&instruction.operands[0], result);
    }

    fn translateNot(self: *Translator, instruction: *const Instruction) !void {
        const dest_size: u32 = 4;
        const src = try self.loadOperand(&instruction.operands[0], dest_size);
        const result = self.allocTemp(dest_size);
        
        try self.emit(.int_not, result, src, null);
        try self.storeOperand(&instruction.operands[0], result);
    }

    fn translateCmp(self: *Translator, instruction: *const Instruction) !void {
        // CMP is like SUB but doesn't store result, only sets flags
        const dest_size: u32 = 4;
        const src1 = try self.loadOperand(&instruction.operands[0], dest_size);
        const src2 = try self.loadOperand(&instruction.operands[1], dest_size);
        const result = self.allocTemp(dest_size);
        
        try self.emit(.int_sub, result, src1, src2);
        // TODO: Set flags based on result
    }

    fn translateTest(self: *Translator, instruction: *const Instruction) !void {
        // TEST is like AND but doesn't store result, only sets flags
        const dest_size: u32 = 4;
        const src1 = try self.loadOperand(&instruction.operands[0], dest_size);
        const src2 = try self.loadOperand(&instruction.operands[1], dest_size);
        const result = self.allocTemp(dest_size);
        
        try self.emit(.int_and, result, src1, src2);
        // TODO: Set flags based on result
    }

    fn translateJmp(self: *Translator, instruction: *const Instruction) !void {
        // Compute target address
        const target = switch (instruction.operands[0]) {
            .relative => |rel| Varnode.constant(@as(u32, @bitCast(@as(i32, @intCast(instruction.address)) + @as(i32, @intCast(instruction.length)) + rel)), 4),
            .immediate => |imm| Varnode.constant(imm, 4),
            else => try self.loadOperand(&instruction.operands[0], 4),
        };
        
        if (instruction.operands[0] == .register or instruction.operands[0] == .memory) {
            try self.emit(.branchind, null, target, null);
        } else {
            try self.emit(.branch, null, target, null);
        }
    }

    fn translateConditionalJmp(self: *Translator, instruction: *const Instruction, cmp_op: OpCode) !void {
        // For conditional jumps, we need to check the flags
        // This is a simplified version - real implementation would track EFLAGS properly
        
        // TODO: Read actual flag values from EFLAGS register
        // For now, we'll emit a placeholder condition
        const condition = self.allocTemp(1);
        try self.emit(cmp_op, condition, Varnode.register(0, 4), Varnode.constant(0, 4));
        
        const target = switch (instruction.operands[0]) {
            .relative => |rel| Varnode.constant(@as(u32, @bitCast(@as(i32, @intCast(instruction.address)) + @as(i32, @intCast(instruction.length)) + rel)), 4),
            .immediate => |imm| Varnode.constant(imm, 4),
            else => try self.loadOperand(&instruction.operands[0], 4),
        };
        
        try self.emit(.cbranch, null, condition, target);
    }

    fn translateConditionalJmpInverted(self: *Translator, instruction: *const Instruction, cmp_op: OpCode) !void {
        // For inverted conditions (JG is !(<=), JA is !(<=)), we need to negate
        const condition_temp = self.allocTemp(1);
        try self.emit(cmp_op, condition_temp, Varnode.register(0, 4), Varnode.constant(0, 4));
        
        const condition = self.allocTemp(1);
        try self.emit(.bool_negate, condition, condition_temp, null);
        
        const target = switch (instruction.operands[0]) {
            .relative => |rel| Varnode.constant(@as(u32, @bitCast(@as(i32, @intCast(instruction.address)) + @as(i32, @intCast(instruction.length)) + rel)), 4),
            .immediate => |imm| Varnode.constant(imm, 4),
            else => try self.loadOperand(&instruction.operands[0], 4),
        };
        
        try self.emit(.cbranch, null, condition, target);
    }

    fn translateCall(self: *Translator, instruction: *const Instruction) !void {
        const target = switch (instruction.operands[0]) {
            .relative => |rel| Varnode.constant(@as(u32, @bitCast(@as(i32, @intCast(instruction.address)) + @as(i32, @intCast(instruction.length)) + rel)), 4),
            .immediate => |imm| Varnode.constant(imm, 4),
            else => try self.loadOperand(&instruction.operands[0], 4),
        };
        
        if (instruction.operands[0] == .register or instruction.operands[0] == .memory) {
            try self.emit(.callind, null, target, null);
        } else {
            try self.emit(.call, null, target, null);
        }
    }

    fn translateRet(self: *Translator, instruction: *const Instruction) !void {
        _ = instruction;
        try self.emit(.@"return", null, null, null);
    }

    /// Get the translated P-code operations
    pub fn getOps(self: *const Translator) []const PCodeOp {
        return self.ops.items;
    }

    /// Clear the operation list for reuse
    pub fn clear(self: *Translator) void {
        self.ops.clearRetainingCapacity();
        self.next_seq = 0;
    }
};

test "translate mov instruction" {
    const allocator = std.testing.allocator;
    var translator = Translator.init(allocator);
    defer translator.deinit();

    // mov eax, 42
    const instruction = Instruction{
        .address = 0x1000,
        .length = 5,
        .mnemonic = .mov,
        .operands = .{
            .{ .register = .eax },
            .{ .immediate = 42 },
            .none,
        },
    };

    try translator.translateInstruction(&instruction);
    const ops = translator.getOps();
    
    try std.testing.expect(ops.len == 1);
    try std.testing.expectEqual(OpCode.copy, ops[0].opcode);
}

test "translate add instruction" {
    const allocator = std.testing.allocator;
    var translator = Translator.init(allocator);
    defer translator.deinit();

    // add eax, ebx
    const instruction = Instruction{
        .address = 0x1000,
        .length = 2,
        .mnemonic = .add,
        .operands = .{
            .{ .register = .eax },
            .{ .register = .ebx },
            .none,
        },
    };

    try translator.translateInstruction(&instruction);
    const ops = translator.getOps();
    
    try std.testing.expect(ops.len == 2); // int_add + copy
    try std.testing.expectEqual(OpCode.int_add, ops[0].opcode);
}
