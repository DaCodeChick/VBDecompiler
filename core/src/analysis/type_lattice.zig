// VB6 Type Lattice - Type system hierarchy for type inference
const std = @import("std");

/// VB6 type categories
pub const VBType = union(enum) {
    // Bottom type (no information yet)
    bottom,
    
    // Numeric types
    byte,           // 8-bit unsigned (0-255)
    boolean,        // 16-bit (True=-1, False=0)
    integer,        // 16-bit signed
    long,           // 32-bit signed
    single,         // 32-bit float
    double_type,    // 64-bit float (named to avoid keyword)
    currency,       // 64-bit fixed-point (scaled by 10000)
    date,           // 64-bit float (days since 1899-12-30)
    
    // String types
    string,         // BSTR (COM string)
    
    // Object types
    object: ObjectType,
    
    // Universal type (can hold any value)
    variant,
    
    // Array types
    array: *ArrayType,
    
    // User-defined types (structures)
    udt: *UDTType,
    
    // Top type (conflicting constraints - error state)
    top,
    
    pub const ObjectType = struct {
        interface_name: ?[]const u8,  // e.g., "IUnknown", "IDispatch", specific class name
        is_dispatch: bool,            // IDispatch interface (late binding)
    };
    
    pub const ArrayType = struct {
        element_type: VBType,
        dimensions: u8,               // VB6 supports multi-dimensional arrays
    };
    
    pub const UDTType = struct {
        name: []const u8,
        fields: []Field,
        
        pub const Field = struct {
            name: []const u8,
            type: VBType,
            offset: u32,
        };
    };
    
    /// Get the size in bytes of a type
    pub fn sizeOf(self: VBType) ?u32 {
        return switch (self) {
            .bottom, .top => null,
            .byte => 1,
            .boolean => 2,
            .integer => 2,
            .long => 4,
            .single => 4,
            .double_type => 8,
            .currency => 8,
            .date => 8,
            .string => 4,        // BSTR pointer
            .object => 4,        // Interface pointer
            .variant => 16,      // VARIANT structure
            .array => 4,         // SAFEARRAY pointer
            .udt => |udt| blk: {
                var size: u32 = 0;
                for (udt.fields) |field| {
                    if (field.type.sizeOf()) |field_size| {
                        size += field_size;
                    } else {
                        break :blk null;
                    }
                }
                break :blk size;
            },
        };
    }
    
    /// Check if this type is numeric
    pub fn isNumeric(self: VBType) bool {
        return switch (self) {
            .byte, .boolean, .integer, .long, .single, .double_type, .currency => true,
            else => false,
        };
    }
    
    /// Check if this type is integral
    pub fn isIntegral(self: VBType) bool {
        return switch (self) {
            .byte, .boolean, .integer, .long => true,
            else => false,
        };
    }
    
    /// Check if this type is floating-point
    pub fn isFloatingPoint(self: VBType) bool {
        return switch (self) {
            .single, .double_type => true,
            else => false,
        };
    }
    
    /// Get the type name as a string
    pub fn typeName(self: VBType) []const u8 {
        return switch (self) {
            .bottom => "⊥",
            .byte => "Byte",
            .boolean => "Boolean",
            .integer => "Integer",
            .long => "Long",
            .single => "Single",
            .double_type => "Double",
            .currency => "Currency",
            .date => "Date",
            .string => "String",
            .object => |obj| {
                if (obj.interface_name) |name| {
                    return name;
                }
                return if (obj.is_dispatch) "Object" else "IUnknown";
            },
            .variant => "Variant",
            .array => "Array",
            .udt => |udt| udt.name,
            .top => "⊤",
        };
    }
};

/// Type lattice operations
pub const TypeLattice = struct {
    /// Join operation: find the least upper bound of two types
    /// Used when a variable has multiple possible types (e.g., from different branches)
    pub fn join(a: VBType, b: VBType) VBType {
        // Bottom is identity for join
        if (a == .bottom) return b;
        if (b == .bottom) return a;
        
        // Same types join to themselves
        if (std.meta.eql(a, b)) return a;
        
        // Top absorbs everything
        if (a == .top or b == .top) return .top;
        
        // Numeric type promotion rules
        if (a.isNumeric() and b.isNumeric()) {
            return numericPromotion(a, b);
        }
        
        // Object types can join if compatible
        if (a == .object and b == .object) {
            return joinObjects(a.object, b.object);
        }
        
        // Variant is compatible with everything (except top)
        if (a == .variant or b == .variant) return .variant;
        
        // Arrays can join if element types are compatible
        if (a == .array and b == .array) {
            if (a.array.dimensions == b.array.dimensions) {
                _ = join(a.array.element_type, b.array.element_type);
                // Would need allocator here in real implementation
                return .variant; // Fallback for now
            }
        }
        
        // Incompatible types -> Variant (VB6's universal type)
        return .variant;
    }
    
    /// Meet operation: find the greatest lower bound of two types
    /// Used when we have constraints that narrow down a type
    pub fn meet(a: VBType, b: VBType) VBType {
        // Bottom absorbs everything
        if (a == .bottom or b == .bottom) return .bottom;
        
        // Top is identity for meet
        if (a == .top) return b;
        if (b == .top) return a;
        
        // Same types meet to themselves
        if (std.meta.eql(a, b)) return a;
        
        // Numeric types: take the more specific one
        if (a.isNumeric() and b.isNumeric()) {
            return numericNarrowing(a, b);
        }
        
        // Incompatible types -> bottom (error state)
        return .bottom;
    }
    
    /// Check if type 'a' is a subtype of 'b' (a ⊆ b)
    pub fn isSubtype(a: VBType, b: VBType) bool {
        // Bottom is subtype of everything
        if (a == .bottom) return true;
        
        // Everything is subtype of top
        if (b == .top) return true;
        
        // Reflexive
        if (std.meta.eql(a, b)) return true;
        
        // Variant is supertype of everything (except top)
        if (b == .variant) return true;
        
        // Numeric subtype rules
        if (a.isNumeric() and b.isNumeric()) {
            return isNumericSubtype(a, b);
        }
        
        return false;
    }
    
    /// Numeric type promotion (for join)
    fn numericPromotion(a: VBType, b: VBType) VBType {
        // Currency has special rules
        if (a == .currency or b == .currency) return .currency;
        
        // Double > Single > Long > Integer > Byte/Boolean
        if (a == .double_type or b == .double_type) return .double_type;
        if (a == .single or b == .single) return .single;
        if (a == .long or b == .long) return .long;
        if (a == .integer or b == .integer) return .integer;
        
        // Both are Byte or Boolean
        if ((a == .byte or a == .boolean) and (b == .byte or b == .boolean)) {
            return .integer; // VB6 promotes to Integer
        }
        
        return .variant;
    }
    
    /// Numeric type narrowing (for meet)
    fn numericNarrowing(a: VBType, b: VBType) VBType {
        // Take the smaller type if one contains the other
        if (a == .byte and b.isIntegral()) return .byte;
        if (b == .byte and a.isIntegral()) return .byte;
        if (a == .integer and (b == .long or b == .integer)) return .integer;
        if (b == .integer and (a == .long or a == .integer)) return .integer;
        
        // Incompatible numeric types
        return .bottom;
    }
    
    /// Check numeric subtype relationship
    fn isNumericSubtype(a: VBType, b: VBType) bool {
        // Byte < Integer < Long
        if (a == .byte and (b == .integer or b == .long)) return true;
        if (a == .integer and b == .long) return true;
        
        // Single < Double
        if (a == .single and b == .double_type) return true;
        
        return false;
    }
    
    /// Join two object types
    fn joinObjects(a: VBType.ObjectType, b: VBType.ObjectType) VBType {
        // If both have same interface, keep it
        if (a.interface_name != null and b.interface_name != null) {
            if (std.mem.eql(u8, a.interface_name.?, b.interface_name.?)) {
                return .{ .object = a };
            }
        }
        
        // If one is IDispatch, result is IDispatch (more general)
        if (a.is_dispatch or b.is_dispatch) {
            return .{ .object = .{ .interface_name = null, .is_dispatch = true } };
        }
        
        // Fall back to generic Object
        return .{ .object = .{ .interface_name = null, .is_dispatch = false } };
    }
};

/// Type inference context
pub const TypeContext = struct {
    allocator: std.mem.Allocator,
    
    pub fn init(allocator: std.mem.Allocator) TypeContext {
        return .{ .allocator = allocator };
    }
    
    /// Create an array type
    pub fn createArrayType(self: *TypeContext, element_type: VBType, dimensions: u8) !VBType {
        const array_type = try self.allocator.create(VBType.ArrayType);
        array_type.* = .{
            .element_type = element_type,
            .dimensions = dimensions,
        };
        return .{ .array = array_type };
    }
    
    /// Create a UDT type
    pub fn createUDTType(self: *TypeContext, name: []const u8, fields: []VBType.UDTType.Field) !VBType {
        const udt_type = try self.allocator.create(VBType.UDTType);
        udt_type.* = .{
            .name = name,
            .fields = fields,
        };
        return .{ .udt = udt_type };
    }
};

test "VB6 type sizes" {
    try std.testing.expectEqual(@as(?u32, 1), VBType.byte.sizeOf());
    try std.testing.expectEqual(@as(?u32, 2), VBType.integer.sizeOf());
    try std.testing.expectEqual(@as(?u32, 4), VBType.long.sizeOf());
    try std.testing.expectEqual(@as(?u32, 4), VBType.single.sizeOf());
    try std.testing.expectEqual(@as(?u32, 8), VBType.double_type.sizeOf());
    try std.testing.expectEqual(@as(?u32, 16), VBType.variant.sizeOf());
}

test "Type lattice join" {
    const result1 = TypeLattice.join(.integer, .long);
    try std.testing.expect(result1 == .long);
    
    const result2 = TypeLattice.join(.single, .double_type);
    try std.testing.expect(result2 == .double_type);
    
    const result3 = TypeLattice.join(.integer, .string);
    try std.testing.expect(result3 == .variant);
    
    const result4 = TypeLattice.join(.bottom, .integer);
    try std.testing.expect(result4 == .integer);
}

test "Type lattice meet" {
    const result1 = TypeLattice.meet(.long, .integer);
    try std.testing.expect(result1 == .integer);
    
    const result2 = TypeLattice.meet(.top, .string);
    try std.testing.expect(result2 == .string);
    
    const result3 = TypeLattice.meet(.integer, .string);
    try std.testing.expect(result3 == .bottom);
}

test "Subtype relationships" {
    try std.testing.expect(TypeLattice.isSubtype(.byte, .integer));
    try std.testing.expect(TypeLattice.isSubtype(.integer, .long));
    try std.testing.expect(TypeLattice.isSubtype(.single, .double_type));
    try std.testing.expect(TypeLattice.isSubtype(.integer, .variant));
    try std.testing.expect(!TypeLattice.isSubtype(.long, .integer));
}
