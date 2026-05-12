// VB6 P-code Disassembler
// Decodes VB6 P-code bytecode into instruction stream

const std = @import("std");
const opcodes = @import("pcode_opcodes.zig");

pub const DisassemblerError = error{
    OutOfBounds,
    InvalidOpcode,
    InvalidOperand,
};

/// VB6 P-code disassembler
pub const PCodeDisassembler = struct {
    allocator: std.mem.Allocator,
    data: []const u8,
    offset: usize,
    
    pub fn init(allocator: std.mem.Allocator, data: []const u8) PCodeDisassembler {
        return .{
            .allocator = allocator,
            .data = data,
            .offset = 0,
        };
    }
    
    /// Reset disassembler to given offset
    pub fn seek(self: *PCodeDisassembler, offset: usize) void {
        self.offset = offset;
    }
    
    /// Decode next instruction
    pub fn decodeNext(self: *PCodeDisassembler) !opcodes.Instruction {
        if (self.offset >= self.data.len) {
            return DisassemblerError.OutOfBounds;
        }
        
        const address = @as(u32, @intCast(self.offset));
        const opcode_byte = self.data[self.offset];
        self.offset += 1;
        
        const opcode = @as(opcodes.Opcode, @enumFromInt(opcode_byte));
        const operand_type = opcodes.getOperandType(opcode);
        
        const operand = try self.readOperand(operand_type);
        
        return opcodes.Instruction{
            .opcode = opcode,
            .address = address,
            .operand_type = operand_type,
            .operand = operand,
        };
    }
    
    /// Read operand based on type
    fn readOperand(self: *PCodeDisassembler, operand_type: opcodes.OperandType) !u32 {
        return switch (operand_type) {
            .None => 0,
            .Imm8, .VarIndex => blk: {
                if (self.offset >= self.data.len) {
                    return DisassemblerError.OutOfBounds;
                }
                const value = self.data[self.offset];
                self.offset += 1;
                break :blk @as(u32, value);
            },
            .Imm16, .Offset, .FuncIndex, .PropIndex => blk: {
                if (self.offset + 2 > self.data.len) {
                    return DisassemblerError.OutOfBounds;
                }
                const value = std.mem.readInt(u16, self.data[self.offset..][0..2], .little);
                self.offset += 2;
                break :blk @as(u32, value);
            },
            .Imm32 => blk: {
                if (self.offset + 4 > self.data.len) {
                    return DisassemblerError.OutOfBounds;
                }
                const value = std.mem.readInt(u32, self.data[self.offset..][0..4], .little);
                self.offset += 4;
                break :blk value;
            },
        };
    }
    
    /// Disassemble entire P-code segment
    pub fn disassembleAll(self: *PCodeDisassembler) !std.ArrayList(opcodes.Instruction) {
        var instructions: std.ArrayList(opcodes.Instruction) = .empty;
        
        self.seek(0);
        while (self.offset < self.data.len) {
            const instr = self.decodeNext() catch |err| {
                if (err == DisassemblerError.OutOfBounds) break;
                return err;
            };
            try instructions.append(self.allocator, instr);
        }
        
        return instructions;
    }
    
    /// Disassemble from address until return or max instructions
    pub fn disassembleFunction(self: *PCodeDisassembler, start: usize, max_instructions: usize) !std.ArrayList(opcodes.Instruction) {
        var instructions: std.ArrayList(opcodes.Instruction) = .empty;
        
        self.seek(start);
        var count: usize = 0;
        
        while (self.offset < self.data.len and count < max_instructions) {
            const instr = self.decodeNext() catch |err| {
                if (err == DisassemblerError.OutOfBounds) break;
                return err;
            };
            
            try instructions.append(self.allocator, instr);
            count += 1;
            
            // Stop at return instruction
            if (instr.opcode == .Ret) break;
        }
        
        return instructions;
    }
};

/// Print P-code instructions
pub fn printInstructions(allocator: std.mem.Allocator, instructions: []const opcodes.Instruction, writer: anytype) !void {
    for (instructions) |instr| {
        const formatted = try instr.format(allocator);
        defer allocator.free(formatted);
        
        try writer.print("  0x{X:0>8}: {s}\n", .{ instr.address, formatted });
    }
}
