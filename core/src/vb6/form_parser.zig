// VB6 Form Parser
// Parses form definitions from PE resources and binary data

const std = @import("std");
const pe = @import("../pe/parser.zig");
const structures = @import("structures.zig");

/// Form parser state
pub const FormParser = struct {
    allocator: std.mem.Allocator,
    pe_file: *const pe.PEFile,
    
    pub fn init(allocator: std.mem.Allocator, pe_file: *const pe.PEFile) FormParser {
        return FormParser{
            .allocator = allocator,
            .pe_file = pe_file,
        };
    }
    
    /// Find all forms in binary
    pub fn findForms(self: *FormParser) !std.ArrayList(FormDefinition) {
        var forms = std.ArrayList(FormDefinition).empty;
        
        // Try to find forms in resources
        const resource_forms = self.findFormsInResources() catch |err| {
            std.debug.print("Resource parsing failed: {}\n", .{err});
            return forms;
        };
        
        for (resource_forms.items) |form| {
            try forms.append(self.allocator, form);
        }
        
        return forms;
    }
    
    /// Find forms in PE resources
    fn findFormsInResources(self: *FormParser) !std.ArrayList(FormDefinition) {
        var forms = std.ArrayList(FormDefinition).empty;
        
        // VB forms can be stored as:
        // 1. RT_RCDATA (Type 10) - binary form data
        // 2. Custom resource types
        // 3. Embedded in .data section
        
        // For now, we'll scan .rsrc section for form-like structures
        _ = self;
        
        return forms;
    }
    
    /// Parse form from data
    pub fn parseForm(self: *FormParser, data: []const u8) !FormDefinition {
        if (data.len < @sizeOf(structures.FormHeader)) {
            return error.InsufficientData;
        }
        
        var header: structures.FormHeader = undefined;
        @memcpy(std.mem.asBytes(&header), data[0..@sizeOf(structures.FormHeader)]);
        
        var form = FormDefinition{
            .name = try self.allocator.dupe(u8, "Form1"),
            .caption = try self.allocator.dupe(u8, ""),
            .left = header.form_position_left,
            .top = header.form_position_top,
            .width = header.form_position_width,
            .height = header.form_position_height,
            .controls = std.ArrayList(structures.ControlInfo).empty,
            .properties = std.StringHashMap([]const u8).init(self.allocator),
        };
        
        // Parse controls (if present)
        var offset = @sizeOf(structures.FormHeader);
        while (offset + @sizeOf(ControlDescriptor) <= data.len) {
            var ctrl_desc: ControlDescriptor = undefined;
            @memcpy(std.mem.asBytes(&ctrl_desc), data[offset..][0..@sizeOf(ControlDescriptor)]);
            
            // Check for control type validity
            if (ctrl_desc.control_type == 0 or ctrl_desc.control_type > 0x100) {
                break;
            }
            
            const control = try self.parseControl(&ctrl_desc, data[offset..]);
            try form.controls.append(self.allocator, control);
            
            offset += @sizeOf(ControlDescriptor);
        }
        
        return form;
    }
    
    /// Parse a single control
    fn parseControl(self: *FormParser, desc: *const ControlDescriptor, data: []const u8) !structures.ControlInfo {
        const ctrl_type_name = controlTypeToString(desc.control_type);
        
        var control = structures.ControlInfo{
            .control_type = try self.allocator.dupe(u8, ctrl_type_name),
            .name = try self.allocator.dupe(u8, "Control1"),
            .left = desc.left,
            .top = desc.top,
            .width = desc.width,
            .height = desc.height,
            .properties = std.StringHashMap([]const u8).init(self.allocator),
        };
        
        // Parse properties from property bag
        if (desc.properties_offset > 0 and desc.properties_offset < data.len) {
            try self.parseProperties(&control, data[desc.properties_offset..]);
        }
        
        return control;
    }
    
    /// Parse control properties
    fn parseProperties(self: *FormParser, control: *structures.ControlInfo, data: []const u8) !void {
        // VB6 property format is typically:
        // [property_id: u16][value_length: u16][value_data]
        
        var offset: usize = 0;
        while (offset + 4 <= data.len) {
            const prop_id = std.mem.readInt(u16, data[offset..][0..2], .little);
            const value_len = std.mem.readInt(u16, data[offset + 2..][0..2], .little);
            offset += 4;
            
            if (value_len == 0 or offset + value_len > data.len) {
                break;
            }
            
            const prop_name = propertyIdToString(prop_id);
            const prop_value = try self.allocator.dupe(u8, data[offset..][0..value_len]);
            
            try control.properties.put(prop_name, prop_value);
            offset += value_len;
        }
    }
};

/// Form definition
pub const FormDefinition = struct {
    name: []const u8,
    caption: []const u8,
    left: i32,
    top: i32,
    width: i32,
    height: i32,
    controls: std.ArrayList(structures.ControlInfo),
    properties: std.StringHashMap([]const u8),
    
    pub fn deinit(self: *FormDefinition) void {
        self.allocator.free(self.name);
        self.allocator.free(self.caption);
        
        for (self.controls.items) |*ctrl| {
            ctrl.deinit(self.allocator);
        }
        self.controls.deinit(self.allocator);
        
        var it = self.properties.iterator();
        while (it.next()) |entry| {
            self.allocator.free(entry.key_ptr.*);
            self.allocator.free(entry.value_ptr.*);
        }
        self.properties.deinit();
    }
};

/// Control descriptor in binary format
const ControlDescriptor = packed struct {
    control_type: u16,
    control_id: u16,
    name_offset: u32,
    caption_offset: u32,
    left: i32,
    top: i32,
    width: i32,
    height: i32,
    properties_offset: u32,
    event_handlers_offset: u32,
};

/// Map control type ID to string
fn controlTypeToString(control_type: u16) []const u8 {
    return switch (control_type) {
        0x01 => "CommandButton",
        0x02 => "Label",
        0x03 => "TextBox",
        0x04 => "Frame",
        0x05 => "CheckBox",
        0x06 => "OptionButton",
        0x07 => "ComboBox",
        0x08 => "ListBox",
        0x09 => "PictureBox",
        0x0A => "Timer",
        0x0B => "HScrollBar",
        0x0C => "VScrollBar",
        0x0D => "Shape",
        0x0E => "Line",
        0x0F => "Image",
        0x10 => "Data",
        0x11 => "Grid",
        0x12 => "Menu",
        else => "Unknown",
    };
}

/// Map property ID to string
fn propertyIdToString(prop_id: u16) []const u8 {
    return switch (prop_id) {
        0x01 => "Caption",
        0x02 => "Text",
        0x03 => "Enabled",
        0x04 => "Visible",
        0x05 => "BackColor",
        0x06 => "ForeColor",
        0x07 => "Font",
        0x08 => "TabIndex",
        0x09 => "TabStop",
        0x0A => "ToolTipText",
        0x0B => "Tag",
        0x0C => "Name",
        else => "Unknown",
    };
}

/// Event mapping
pub const EventMapping = struct {
    control_name: []const u8,
    event_name: []const u8,
    handler_address: u32,
    
    pub fn deinit(self: *EventMapping, allocator: std.mem.Allocator) void {
        allocator.free(self.control_name);
        allocator.free(self.event_name);
    }
};

/// Parse event handlers
pub fn parseEventHandlers(allocator: std.mem.Allocator, pe_file: *const pe.PEFile, event_table_rva: u32) !std.ArrayList(EventMapping) {
    var events = std.ArrayList(EventMapping).empty;
    
    // Event handler table format:
    // [control_id: u16][event_id: u16][handler_address: u32]
    
    var offset: u32 = 0;
    while (true) {
        const data = pe_file.rvaToData(event_table_rva + offset, 8) orelse break;
        
        const control_id = std.mem.readInt(u16, data[0..2], .little);
        const event_id = std.mem.readInt(u16, data[2..4], .little);
        const handler_addr = std.mem.readInt(u32, data[4..8], .little);
        
        if (control_id == 0 and event_id == 0 and handler_addr == 0) {
            break;
        }
        
        const event_name = eventIdToString(event_id);
        
        const event_mapping = EventMapping{
            .control_name = try std.fmt.allocPrint(allocator, "Control{d}", .{control_id}),
            .event_name = try allocator.dupe(u8, event_name),
            .handler_address = handler_addr,
        };
        
        try events.append(allocator, event_mapping);
        offset += 8;
        
        if (events.items.len > 1000) {
            break; // Safety limit
        }
    }
    
    return events;
}

/// Map event ID to string
fn eventIdToString(event_id: u16) []const u8 {
    return switch (event_id) {
        0x01 => "Click",
        0x02 => "DblClick",
        0x03 => "Load",
        0x04 => "Unload",
        0x05 => "Initialize",
        0x06 => "Terminate",
        0x07 => "KeyPress",
        0x08 => "KeyDown",
        0x09 => "KeyUp",
        0x0A => "MouseDown",
        0x0B => "MouseUp",
        0x0C => "MouseMove",
        0x0D => "Change",
        0x0E => "GotFocus",
        0x0F => "LostFocus",
        0x10 => "Resize",
        0x11 => "Paint",
        else => "Unknown",
    };
}
