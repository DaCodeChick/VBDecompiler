// Variable Naming - Generate meaningful variable names for decompiled code

const std = @import("std");
const VBType = @import("../analysis/type_lattice.zig").VBType;
const SSAFunction = @import("../ir/ssa.zig").SSAFunction;
const TypeInferenceEngine = @import("../analysis/type_inference.zig").TypeInferenceEngine;

/// Variable naming strategy
pub const VariableNamer = struct {
    allocator: std.mem.Allocator,
    type_engine: *const TypeInferenceEngine,
    names: std.AutoHashMap(u64, []const u8),
    name_counts: std.StringHashMap(u32),
    
    pub fn init(allocator: std.mem.Allocator, type_engine: *const TypeInferenceEngine) VariableNamer {
        return .{
            .allocator = allocator,
            .type_engine = type_engine,
            .names = std.AutoHashMap(u64, []const u8).init(allocator),
            .name_counts = std.StringHashMap(u32).init(allocator),
        };
    }
    
    pub fn deinit(self: *VariableNamer) void {
        var it = self.names.valueIterator();
        while (it.next()) |name| {
            self.allocator.free(name.*);
        }
        self.names.deinit();
        self.name_counts.deinit();
    }
    
    /// Generate a name for a variable
    pub fn generateName(self: *VariableNamer, var_id: u64, vb_type: VBType) ![]const u8 {
        // Check if already named
        if (self.names.get(var_id)) |name| {
            return name;
        }
        
        // Generate base name from type
        const base = self.typeToBaseName(vb_type);
        
        // Make it unique
        const count = self.name_counts.get(base) orelse 0;
        try self.name_counts.put(base, count + 1);
        
        const name = if (count == 0)
            try std.fmt.allocPrint(self.allocator, "{s}", .{base})
        else
            try std.fmt.allocPrint(self.allocator, "{s}{d}", .{ base, count });
        
        try self.names.put(var_id, name);
        return name;
    }
    
    /// Get base name from VB type
    fn typeToBaseName(self: *VariableNamer, vb_type: VBType) []const u8 {
        _ = self;
        return switch (vb_type) {
            .byte => "b",
            .boolean => "flag",
            .integer => "i",
            .long => "l",
            .single => "f",
            .double_type => "d",
            .currency => "c",
            .date => "dt",
            .string => "str",
            .object => "obj",
            .variant => "v",
            .array => "arr",
            .udt => "udt",
            else => "var",
        };
    }
};
