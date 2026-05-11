// x86 instruction decoder
const std = @import("std");
const instruction = @import("instruction.zig");
const Instruction = instruction.Instruction;
const Mnemonic = instruction.Mnemonic;
const Operand = instruction.Operand;
const Register = instruction.Register;
const Prefix = instruction.Prefix;
const ModRM = instruction.ModRM;
const SIB = instruction.SIB;
const MemoryOperand = instruction.MemoryOperand;

pub const DecodeError = error{
    InvalidOpcode,
    TruncatedInstruction,
    OutOfBounds,
};

pub const Decoder = struct {
    data: []const u8,
    pos: usize,
    address: u32,
    
    pub fn init(data: []const u8, address: u32) Decoder {
        return Decoder{
            .data = data,
            .pos = 0,
            .address = address,
        };
    }
    
    pub fn decode(self: *Decoder) DecodeError!Instruction {
        const start_pos = self.pos;
        const inst_addr = self.address + @as(u32, @intCast(self.pos));
        
        // Parse prefixes
        var prefix = Prefix{};
        while (self.pos < self.data.len) {
            const byte = self.data[self.pos];
            const has_prefix = switch (byte) {
                0xF0 => blk: { prefix.lock = true; break :blk true; },
                0xF2 => blk: { prefix.repne = true; break :blk true; },
                0xF3 => blk: { prefix.rep = true; break :blk true; },
                0x2E => blk: { prefix.cs_override = true; break :blk true; },
                0x36 => blk: { prefix.ss_override = true; break :blk true; },
                0x3E => blk: { prefix.ds_override = true; break :blk true; },
                0x26 => blk: { prefix.es_override = true; break :blk true; },
                0x64 => blk: { prefix.fs_override = true; break :blk true; },
                0x65 => blk: { prefix.gs_override = true; break :blk true; },
                0x66 => blk: { prefix.operand_size = true; break :blk true; },
                0x67 => blk: { prefix.address_size = true; break :blk true; },
                else => false,
            };
            
            if (!has_prefix) break;
            self.pos += 1;
        }
        
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        
        // Get opcode
        const opcode = self.data[self.pos];
        self.pos += 1;
        
        // Decode instruction based on opcode
        var inst = Instruction{
            .address = inst_addr,
            .length = 0,
            .prefix = prefix,
            .mnemonic = .invalid,
        };
        
        // Copy bytes
        const inst_len = self.pos - start_pos;
        @memcpy(inst.bytes[0..inst_len], self.data[start_pos..self.pos]);
        
        try self.decodeOpcode(opcode, &inst);
        
        inst.length = @intCast(self.pos - start_pos);
        return inst;
    }
    
    fn decodeOpcode(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        switch (opcode) {
            // MOV immediate to register (B0-BF)
            0xB0...0xB7 => try self.decodeMovImmToReg8(opcode, inst),
            0xB8...0xBF => try self.decodeMovImmToReg32(opcode, inst),
            
            // MOV r/m to/from register (88-8B)
            0x88 => try self.decodeMovRegToRM8(inst),
            0x89 => try self.decodeMovRegToRM32(inst),
            0x8A => try self.decodeMovRMToReg8(inst),
            0x8B => try self.decodeMovRMToReg32(inst),
            
            // PUSH/POP register (50-5F)
            0x50...0x57 => try self.decodePushReg(opcode, inst),
            0x58...0x5F => try self.decodePopReg(opcode, inst),
            
            // ADD/SUB/AND/OR/XOR with immediate (80-83)
            0x80 => try self.decodeALUImm8(inst),
            0x81 => try self.decodeALUImm32(inst),
            0x83 => try self.decodeALUImm8Sx(inst),
            
            // ADD r/m to/from register
            0x00 => try self.decodeAddRegToRM8(inst),
            0x01 => try self.decodeAddRegToRM32(inst),
            0x02 => try self.decodeAddRMToReg8(inst),
            0x03 => try self.decodeAddRMToReg32(inst),
            
            // INC/DEC register (40-4F)
            0x40...0x47 => try self.decodeIncReg(opcode, inst),
            0x48...0x4F => try self.decodeDecReg(opcode, inst),
            
            // JMP/CALL near relative
            0xE8 => try self.decodeCallNear(inst),
            0xE9 => try self.decodeJmpNear(inst),
            0xEB => try self.decodeJmpShort(inst),
            
            // Conditional jumps (70-7F, 0F 80-8F)
            0x70...0x7F => try self.decodeJccShort(opcode, inst),
            
            // RET
            0xC2 => try self.decodeRetImm(inst),
            0xC3 => try self.decodeRetNear(inst),
            
            // TEST
            0x84 => try self.decodeTestReg8(inst),
            0x85 => try self.decodeTestReg32(inst),
            
            // CMP
            0x38 => try self.decodeCmpRegToRM8(inst),
            0x39 => try self.decodeCmpRegToRM32(inst),
            0x3A => try self.decodeCmpRMToReg8(inst),
            0x3B => try self.decodeCmpRMToReg32(inst),
            
            // XOR r/m with register
            0x30 => try self.decodeXorRegToRM8(inst),
            0x31 => try self.decodeXorRegToRM32(inst),
            0x32 => try self.decodeXorRMToReg8(inst),
            0x33 => try self.decodeXorRMToReg32(inst),
            
            // NOP
            0x90 => {
                inst.mnemonic = .nop;
            },
            
            // INT
            0xCC => {
                inst.mnemonic = .int;
                inst.operands[0] = .{ .immediate = 3 };
            },
            0xCD => {
                inst.mnemonic = .int;
                if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
                inst.operands[0] = .{ .immediate = self.data[self.pos] };
                self.pos += 1;
            },
            
            // LEA
            0x8D => try self.decodeLeaRegMem(inst),
            
            // SUB r/m with register
            0x28 => try self.decodeSubRegToRM8(inst),
            0x29 => try self.decodeSubRegToRM32(inst),
            0x2A => try self.decodeSubRMToReg8(inst),
            0x2B => try self.decodeSubRMToReg32(inst),
            
            // OR r/m with register
            0x08 => try self.decodeOrRegToRM8(inst),
            0x09 => try self.decodeOrRegToRM32(inst),
            0x0A => try self.decodeOrRMToReg8(inst),
            0x0B => try self.decodeOrRMToReg32(inst),
            
            // AND r/m with register
            0x20 => try self.decodeAndRegToRM8(inst),
            0x21 => try self.decodeAndRegToRM32(inst),
            0x22 => try self.decodeAndRMToReg8(inst),
            0x23 => try self.decodeAndRMToReg32(inst),
            
            // MOVZX/MOVSX (0F B6, 0F B7, 0F BE, 0F BF)
            // These are handled in decodeTwoByteOpcode
            
            // Two-byte opcodes (0F prefix)
            0x0F => try self.decodeTwoByteOpcode(inst),
            
            else => {
                inst.mnemonic = .invalid;
            },
        }
    }
    
    fn decodeMovImmToReg8(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .mov;
        const reg_idx = opcode & 0x07;
        inst.operands[0] = .{ .register = @enumFromInt(reg_idx) };
        
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        inst.operands[1] = .{ .immediate = self.data[self.pos] };
        self.pos += 1;
    }
    
    fn decodeMovImmToReg32(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .mov;
        const reg_idx = opcode & 0x07;
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) reg_idx + 8 else reg_idx + 16) };
        
        if (use_16bit) {
            if (self.pos + 2 > self.data.len) return DecodeError.TruncatedInstruction;
            inst.operands[1] = .{ .immediate = std.mem.readInt(u16, self.data[self.pos..][0..2], .little) };
            self.pos += 2;
        } else {
            if (self.pos + 4 > self.data.len) return DecodeError.TruncatedInstruction;
            inst.operands[1] = .{ .immediate = std.mem.readInt(u32, self.data[self.pos..][0..4], .little) };
            self.pos += 4;
        }
    }
    
    fn decodeMovRegToRM32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .mov;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeMovRegToRM8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .mov;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeMovRMToReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .mov;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeMovRMToReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .mov;
        const modrm = try self.readModRM();
        
        inst.operands[0] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
        inst.operands[1] = try self.decodeRM(modrm, 8);
    }
    
    fn decodePushReg(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        _ = self;
        inst.mnemonic = .push;
        const reg_idx = opcode & 0x07;
        const use_16bit = inst.prefix.operand_size;
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) reg_idx + 8 else reg_idx + 16) };
    }
    
    fn decodePopReg(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        _ = self;
        inst.mnemonic = .pop;
        const reg_idx = opcode & 0x07;
        const use_16bit = inst.prefix.operand_size;
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) reg_idx + 8 else reg_idx + 16) };
    }
    
    fn decodeIncReg(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        _ = self;
        inst.mnemonic = .inc;
        const reg_idx = opcode & 0x07;
        const use_16bit = inst.prefix.operand_size;
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) reg_idx + 8 else reg_idx + 16) };
    }
    
    fn decodeDecReg(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        _ = self;
        inst.mnemonic = .dec;
        const reg_idx = opcode & 0x07;
        const use_16bit = inst.prefix.operand_size;
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) reg_idx + 8 else reg_idx + 16) };
    }
    
    fn decodeCallNear(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .call;
        if (self.pos + 4 > self.data.len) return DecodeError.TruncatedInstruction;
        
        const offset = std.mem.readInt(i32, self.data[self.pos..][0..4], .little);
        self.pos += 4;
        
        const target: i32 = @as(i32, @intCast(inst.address)) + @as(i32, @intCast(self.pos - (self.pos - 4) + 5)) + offset;
        inst.operands[0] = .{ .relative = target };
    }
    
    fn decodeJmpNear(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .jmp;
        if (self.pos + 4 > self.data.len) return DecodeError.TruncatedInstruction;
        
        const offset = std.mem.readInt(i32, self.data[self.pos..][0..4], .little);
        self.pos += 4;
        
        const target: i32 = @as(i32, @intCast(inst.address)) + @as(i32, @intCast(self.pos - (self.pos - 4) + 5)) + offset;
        inst.operands[0] = .{ .relative = target };
    }
    
    fn decodeJmpShort(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .jmp;
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        
        const offset: i8 = @bitCast(self.data[self.pos]);
        self.pos += 1;
        
        const target: i32 = @as(i32, @intCast(inst.address)) + @as(i32, @intCast(self.pos - (self.pos - 1) + 2)) + offset;
        inst.operands[0] = .{ .relative = target };
    }
    
    fn decodeJccShort(self: *Decoder, opcode: u8, inst: *Instruction) DecodeError!void {
        // 70-7F: conditional jumps
        inst.mnemonic = switch (opcode) {
            0x70 => .jo,
            0x71 => .jno,
            0x72 => .jb,
            0x73 => .jae,
            0x74 => .je,
            0x75 => .jne,
            0x76 => .jbe,
            0x77 => .ja,
            0x78 => .js,
            0x79 => .jns,
            0x7C => .jl,
            0x7D => .jge,
            0x7E => .jle,
            0x7F => .jg,
            else => .invalid,
        };
        
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        
        const offset: i8 = @bitCast(self.data[self.pos]);
        self.pos += 1;
        
        const target: i32 = @as(i32, @intCast(inst.address)) + @as(i32, @intCast(self.pos - (self.pos - 1) + 2)) + offset;
        inst.operands[0] = .{ .relative = target };
    }
    
    fn decodeRetNear(self: *Decoder, inst: *Instruction) DecodeError!void {
        _ = self;
        inst.mnemonic = .ret;
    }
    
    fn decodeRetImm(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .ret;
        if (self.pos + 2 > self.data.len) return DecodeError.TruncatedInstruction;
        inst.operands[0] = .{ .immediate = std.mem.readInt(u16, self.data[self.pos..][0..2], .little) };
        self.pos += 2;
    }
    
    fn decodeALUImm8(self: *Decoder, inst: *Instruction) DecodeError!void {
        const modrm = try self.readModRM();
        inst.mnemonic = switch (modrm.reg) {
            0 => .add,
            1 => .or_,
            2 => .adc,
            3 => .sbb,
            4 => .and_,
            5 => .sub,
            6 => .xor,
            7 => .cmp,
        };
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        inst.operands[1] = .{ .immediate = self.data[self.pos] };
        self.pos += 1;
    }
    
    fn decodeALUImm32(self: *Decoder, inst: *Instruction) DecodeError!void {
        const modrm = try self.readModRM();
        inst.mnemonic = switch (modrm.reg) {
            0 => .add,
            1 => .or_,
            2 => .adc,
            3 => .sbb,
            4 => .and_,
            5 => .sub,
            6 => .xor,
            7 => .cmp,
        };
        
        const use_16bit = inst.prefix.operand_size;
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        
        if (use_16bit) {
            if (self.pos + 2 > self.data.len) return DecodeError.TruncatedInstruction;
            inst.operands[1] = .{ .immediate = std.mem.readInt(u16, self.data[self.pos..][0..2], .little) };
            self.pos += 2;
        } else {
            if (self.pos + 4 > self.data.len) return DecodeError.TruncatedInstruction;
            inst.operands[1] = .{ .immediate = std.mem.readInt(u32, self.data[self.pos..][0..4], .little) };
            self.pos += 4;
        }
    }
    
    fn decodeALUImm8Sx(self: *Decoder, inst: *Instruction) DecodeError!void {
        const modrm = try self.readModRM();
        inst.mnemonic = switch (modrm.reg) {
            0 => .add,
            1 => .or_,
            2 => .adc,
            3 => .sbb,
            4 => .and_,
            5 => .sub,
            6 => .xor,
            7 => .cmp,
        };
        
        const use_16bit = inst.prefix.operand_size;
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        const imm8: i8 = @bitCast(self.data[self.pos]);
        inst.operands[1] = .{ .immediate = @bitCast(@as(i32, imm8)) };
        self.pos += 1;
    }
    
    fn decodeAddRegToRM32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .add;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeAddRegToRM8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .add;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeAddRMToReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .add;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeAddRMToReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .add;
        const modrm = try self.readModRM();
        
        inst.operands[0] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
        inst.operands[1] = try self.decodeRM(modrm, 8);
    }
    
    fn decodeTestReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .test_;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeTestReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .test_;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeCmpRegToRM32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .cmp;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeCmpRegToRM8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .cmp;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeCmpRMToReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .cmp;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeCmpRMToReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .cmp;
        const modrm = try self.readModRM();
        
        inst.operands[0] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
        inst.operands[1] = try self.decodeRM(modrm, 8);
    }
    
    fn decodeXorRegToRM32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .xor;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeXorRegToRM8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .xor;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeXorRMToReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .xor;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeXorRMToReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .xor;
        const modrm = try self.readModRM();
        
        inst.operands[0] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
        inst.operands[1] = try self.decodeRM(modrm, 8);
    }
    
    fn decodeLeaRegMem(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .lea;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeSubRegToRM32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .sub;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeSubRegToRM8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .sub;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeSubRMToReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .sub;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeSubRMToReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .sub;
        const modrm = try self.readModRM();
        
        inst.operands[0] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
        inst.operands[1] = try self.decodeRM(modrm, 8);
    }
    
    fn decodeOrRegToRM32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .or_;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeOrRegToRM8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .or_;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeOrRMToReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .or_;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeOrRMToReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .or_;
        const modrm = try self.readModRM();
        
        inst.operands[0] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
        inst.operands[1] = try self.decodeRM(modrm, 8);
    }
    
    fn decodeAndRegToRM32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .and_;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
        inst.operands[1] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
    }
    
    fn decodeAndRegToRM8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .and_;
        const modrm = try self.readModRM();
        
        inst.operands[0] = try self.decodeRM(modrm, 8);
        inst.operands[1] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
    }
    
    fn decodeAndRMToReg32(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .and_;
        const modrm = try self.readModRM();
        const use_16bit = inst.prefix.operand_size;
        
        inst.operands[0] = .{ .register = @enumFromInt(if (use_16bit) @as(u8, modrm.reg) + 8 else @as(u8, modrm.reg) + 16) };
        inst.operands[1] = try self.decodeRM(modrm, if (use_16bit) 16 else 32);
    }
    
    fn decodeAndRMToReg8(self: *Decoder, inst: *Instruction) DecodeError!void {
        inst.mnemonic = .and_;
        const modrm = try self.readModRM();
        
        inst.operands[0] = .{ .register = @enumFromInt(@as(u8, modrm.reg)) };
        inst.operands[1] = try self.decodeRM(modrm, 8);
    }
    
    fn decodeTwoByteOpcode(self: *Decoder, inst: *Instruction) DecodeError!void {
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        
        const opcode2 = self.data[self.pos];
        self.pos += 1;
        
        switch (opcode2) {
            // Conditional moves (0F 40-4F)
            // Long conditional jumps (0F 80-8F)
            0x80...0x8F => {
                inst.mnemonic = switch (opcode2) {
                    0x80 => .jo,
                    0x81 => .jno,
                    0x82 => .jb,
                    0x83 => .jae,
                    0x84 => .je,
                    0x85 => .jne,
                    0x86 => .jbe,
                    0x87 => .ja,
                    0x88 => .js,
                    0x89 => .jns,
                    0x8C => .jl,
                    0x8D => .jge,
                    0x8E => .jle,
                    0x8F => .jg,
                    else => .invalid,
                };
                
                if (self.pos + 4 > self.data.len) return DecodeError.TruncatedInstruction;
                const offset = std.mem.readInt(i32, self.data[self.pos..][0..4], .little);
                self.pos += 4;
                
                const target: i32 = @as(i32, @intCast(inst.address)) + @as(i32, @intCast(self.pos + 6)) + offset;
                inst.operands[0] = .{ .relative = target };
            },
            else => {
                inst.mnemonic = .invalid;
            },
        }
    }
    
    fn readModRM(self: *Decoder) DecodeError!ModRM {
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        const byte = self.data[self.pos];
        self.pos += 1;
        return @bitCast(byte);
    }
    
    fn readSIB(self: *Decoder) DecodeError!SIB {
        if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
        const byte = self.data[self.pos];
        self.pos += 1;
        return @bitCast(byte);
    }
    
    fn decodeRM(self: *Decoder, modrm: ModRM, bit_size: u8) DecodeError!Operand {
        // If mod == 11, it's a register
        if (modrm.mod == 3) {
            const base_offset: u8 = switch (bit_size) {
                8 => 0,
                16 => 8,
                32 => 16,
                else => 0,
            };
            return .{ .register = @enumFromInt(@as(u8, modrm.rm) + base_offset) };
        }
        
        // Otherwise, it's a memory operand
        var mem = MemoryOperand{};
        
        // Check if we need SIB byte
        if (modrm.rm == 4 and modrm.mod != 3) {
            const sib = try self.readSIB();
            
            if (sib.base != 5 or modrm.mod != 0) {
                mem.base = @enumFromInt(@as(u8, sib.base) + 16); // 32-bit base
            }
            
            if (sib.index != 4) {
                mem.index = @enumFromInt(@as(u8, sib.index) + 16); // 32-bit index
                mem.scale = @as(u8, 1) << sib.scale;
            }
        } else {
            // No SIB, use rm as base register
            if (modrm.rm != 5 or modrm.mod != 0) {
                mem.base = @enumFromInt(@as(u8, modrm.rm) + 16); // 32-bit base
            }
        }
        
        // Displacement
        switch (modrm.mod) {
            0 => {
                // No displacement, unless rm == 5 (disp32)
                if (modrm.rm == 5) {
                    if (self.pos + 4 > self.data.len) return DecodeError.TruncatedInstruction;
                    mem.displacement = @bitCast(std.mem.readInt(u32, self.data[self.pos..][0..4], .little));
                    mem.has_displacement = true;
                    self.pos += 4;
                }
            },
            1 => {
                // disp8
                if (self.pos >= self.data.len) return DecodeError.TruncatedInstruction;
                const disp8: i8 = @bitCast(self.data[self.pos]);
                mem.displacement = @as(i32, disp8);
                mem.has_displacement = true;
                self.pos += 1;
            },
            2 => {
                // disp32
                if (self.pos + 4 > self.data.len) return DecodeError.TruncatedInstruction;
                mem.displacement = @bitCast(std.mem.readInt(u32, self.data[self.pos..][0..4], .little));
                mem.has_displacement = true;
                self.pos += 4;
            },
            else => {},
        }
        
        return .{ .memory = mem };
    }
};

test "decode mov eax, 1" {
    const data = [_]u8{ 0xB8, 0x01, 0x00, 0x00, 0x00 };
    var decoder = Decoder.init(&data, 0x401000);
    
    const inst = try decoder.decode();
    try std.testing.expectEqual(Mnemonic.mov, inst.mnemonic);
    try std.testing.expectEqual(@as(u8, 5), inst.length);
    try std.testing.expectEqual(Register.eax, inst.operands[0].register);
    try std.testing.expectEqual(@as(u32, 1), inst.operands[1].immediate);
}

test "decode push ebp" {
    const data = [_]u8{0x55};
    var decoder = Decoder.init(&data, 0x401000);
    
    const inst = try decoder.decode();
    try std.testing.expectEqual(Mnemonic.push, inst.mnemonic);
    try std.testing.expectEqual(@as(u8, 1), inst.length);
    try std.testing.expectEqual(Register.ebp, inst.operands[0].register);
}

test "decode ret" {
    const data = [_]u8{0xC3};
    var decoder = Decoder.init(&data, 0x401000);
    
    const inst = try decoder.decode();
    try std.testing.expectEqual(Mnemonic.ret, inst.mnemonic);
    try std.testing.expectEqual(@as(u8, 1), inst.length);
}
