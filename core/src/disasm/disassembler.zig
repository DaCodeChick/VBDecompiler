// Linear-sweep disassembler
const std = @import("std");
const Decoder = @import("decoder.zig").Decoder;
const Instruction = @import("instruction.zig").Instruction;

pub const DisassemblerError = error{
    OutOfMemory,
    InvalidAddress,
    DecodeFailed,
};

pub const DisassemblerOptions = struct {
    /// Start address for disassembly
    start_address: u32,
    /// End address (exclusive)
    end_address: u32,
    /// Maximum number of instructions to decode (0 = no limit)
    max_instructions: usize = 0,
};

pub const Disassembler = struct {
    allocator: std.mem.Allocator,
    data: []const u8,
    base_address: u32,
    
    pub fn init(allocator: std.mem.Allocator, data: []const u8, base_address: u32) Disassembler {
        return Disassembler{
            .allocator = allocator,
            .data = data,
            .base_address = base_address,
        };
    }
    
    /// Disassemble a region of code using linear sweep
    pub fn disassemble(self: *Disassembler, options: DisassemblerOptions) DisassemblerError![]Instruction {
        // Validate address range
        if (options.start_address < self.base_address) return DisassemblerError.InvalidAddress;
        
        const start_offset = options.start_address - self.base_address;
        const end_offset = options.end_address - self.base_address;
        
        if (start_offset >= self.data.len or end_offset > self.data.len) {
            return DisassemblerError.InvalidAddress;
        }
        
        // Create instruction list
        var instructions: std.ArrayList(Instruction) = .{ .items = &.{}, .capacity = 0 };
        errdefer instructions.deinit(self.allocator);
        
        // Create decoder
        var decoder = Decoder.init(self.data[start_offset..end_offset], options.start_address);
        
        // Decode instructions until we reach the end or max count
        while (decoder.pos < decoder.data.len) {
            // Check max instruction limit
            if (options.max_instructions > 0 and instructions.items.len >= options.max_instructions) {
                break;
            }
            
            // Decode next instruction
            const inst = decoder.decode() catch {
                // On decode error, skip this byte and continue
                // This is a simple recovery strategy for linear sweep
                decoder.pos += 1;
                continue;
            };
            
            try instructions.append(self.allocator, inst);
            
            // Stop at return or invalid instruction
            if (inst.mnemonic == .ret or inst.mnemonic == .invalid) {
                break;
            }
        }
        
        return instructions.toOwnedSlice(self.allocator);
    }
    
    /// Disassemble a single function starting at the given address
    /// This is a simple implementation that stops at the first RET
    pub fn disassembleFunction(self: *Disassembler, address: u32) DisassemblerError![]Instruction {
        if (address < self.base_address) return DisassemblerError.InvalidAddress;
        
        const offset = address - self.base_address;
        if (offset >= self.data.len) return DisassemblerError.InvalidAddress;
        
        var instructions: std.ArrayList(Instruction) = .{ .items = &.{}, .capacity = 0 };
        errdefer instructions.deinit(self.allocator);
        
        var decoder = Decoder.init(self.data[offset..], address);
        
        // Decode until we hit a RET or invalid instruction
        while (decoder.pos < decoder.data.len) {
            const inst = decoder.decode() catch {
                decoder.pos += 1;
                continue;
            };
            
            try instructions.append(self.allocator, inst);
            
            // Stop at return or invalid instruction
            if (inst.mnemonic == .ret or inst.mnemonic == .invalid) {
                break;
            }
        }
        
        return instructions.toOwnedSlice(self.allocator);
    }
    
    /// Free instruction list returned by disassemble()
    pub fn freeInstructions(self: *Disassembler, instructions: []Instruction) void {
        self.allocator.free(instructions);
    }
};

test "disassemble simple function" {
    // Simple function: push ebp, mov ebp esp, pop ebp, ret
    const data = [_]u8{
        0x55,             // push ebp
        0x89, 0xE5,       // mov ebp, esp
        0x5D,             // pop ebp
        0xC3,             // ret
    };
    
    var disasm = Disassembler.init(std.testing.allocator, &data, 0x401000);
    
    const instructions = try disasm.disassembleFunction(0x401000);
    defer disasm.freeInstructions(instructions);
    
    try std.testing.expectEqual(@as(usize, 4), instructions.len);
    try std.testing.expectEqual(@as(u32, 0x401000), instructions[0].address);
    try std.testing.expectEqual(@as(u32, 0x401001), instructions[1].address);
    try std.testing.expectEqual(@as(u32, 0x401003), instructions[2].address);
    try std.testing.expectEqual(@as(u32, 0x401004), instructions[3].address);
}

test "disassemble with range" {
    const data = [_]u8{
        0xB8, 0x01, 0x00, 0x00, 0x00,  // mov eax, 1
        0xB9, 0x02, 0x00, 0x00, 0x00,  // mov ecx, 2
        0xC3,                           // ret
    };
    
    var disasm = Disassembler.init(std.testing.allocator, &data, 0x401000);
    
    const options = DisassemblerOptions{
        .start_address = 0x401000,
        .end_address = 0x40100B,
    };
    
    const instructions = try disasm.disassemble(options);
    defer disasm.freeInstructions(instructions);
    
    try std.testing.expectEqual(@as(usize, 3), instructions.len);
}
