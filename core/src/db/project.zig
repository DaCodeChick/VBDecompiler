// Project database management for VBDecompiler
// Handles creation, initialization, and management of project databases

const std = @import("std");
const sqlite = @import("sqlite.zig");
const cfg = @import("../analysis/cfg.zig");
const type_lattice = @import("../analysis/type_lattice.zig");

pub const ProjectError = error{
    CreateFailed,
    InitFailed,
    LoadFailed,
    SaveFailed,
};

/// Project database wrapper
pub const Project = struct {
    db: sqlite.Database,
    allocator: std.mem.Allocator,
    binary_path: []const u8,
    
    /// Create a new project database
    pub fn create(allocator: std.mem.Allocator, project_path: []const u8, binary_path: []const u8) !Project {
        var db = try sqlite.Database.open(allocator, project_path);
        errdefer db.close();
        
        // Read schema from embedded file or initialize tables
        try initializeSchema(&db);
        
        // Store binary path
        const path_dup = try allocator.dupe(u8, binary_path);
        
        return Project{
            .db = db,
            .allocator = allocator,
            .binary_path = path_dup,
        };
    }
    
    /// Open an existing project database
    pub fn open(allocator: std.mem.Allocator, project_path: []const u8) !Project {
        var db = try sqlite.Database.open(allocator, project_path);
        errdefer db.close();
        
        // Load binary path from project_info
        var stmt = try db.prepare("SELECT value FROM project_info WHERE key = 'binary_path'");
        defer stmt.finalize();
        
        const has_row = try stmt.step();
        if (!has_row) {
            return ProjectError.LoadFailed;
        }
        
        const binary_path = try allocator.dupe(u8, stmt.columnText(0));
        
        return Project{
            .db = db,
            .allocator = allocator,
            .binary_path = binary_path,
        };
    }
    
    /// Close the project
    pub fn close(self: *Project) void {
        self.allocator.free(self.binary_path);
        self.db.close();
    }
    
    /// Save binary information
    pub fn saveBinaryInfo(self: *Project, info: BinaryInfo) !void {
        var stmt = try self.db.prepare(
            \\INSERT INTO binary_info (file_path, file_hash, image_base, entry_point, is_vb6, vb_version, compilation_type, binary_type)
            \\VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        );
        defer stmt.finalize();
        
        try stmt.bindText(1, info.file_path);
        try stmt.bindText(2, info.file_hash);
        try stmt.bindInt(3, @intCast(info.image_base));
        try stmt.bindInt(4, @intCast(info.entry_point));
        try stmt.bindInt(5, if (info.is_vb6) 1 else 0);
        
        if (info.vb_version) |ver| {
            try stmt.bindText(6, ver);
        } else {
            try stmt.bindNull(6);
        }
        
        if (info.compilation_type) |ct| {
            try stmt.bindText(7, ct);
        } else {
            try stmt.bindNull(7);
        }
        
        if (info.binary_type) |bt| {
            try stmt.bindText(8, bt);
        } else {
            try stmt.bindNull(8);
        }
        
        _ = try stmt.step();
    }
    
    /// Save a function
    pub fn saveFunction(self: *Project, func: Function) !i64 {
        var stmt = try self.db.prepare(
            \\INSERT OR REPLACE INTO functions (address, name, type, is_analyzed, is_decompiled, signature, return_type)
            \\VALUES (?, ?, ?, ?, ?, ?, ?)
        );
        defer stmt.finalize();
        
        try stmt.bindInt(1, @intCast(func.address));
        
        if (func.name) |name| {
            try stmt.bindText(2, name);
        } else {
            try stmt.bindNull(2);
        }
        
        if (func.func_type) |ft| {
            try stmt.bindText(3, ft);
        } else {
            try stmt.bindNull(3);
        }
        
        try stmt.bindInt(4, if (func.is_analyzed) 1 else 0);
        try stmt.bindInt(5, if (func.is_decompiled) 1 else 0);
        
        if (func.signature) |sig| {
            try stmt.bindText(6, sig);
        } else {
            try stmt.bindNull(6);
        }
        
        if (func.return_type) |rt| {
            try stmt.bindText(7, rt);
        } else {
            try stmt.bindNull(7);
        }
        
        _ = try stmt.step();
        return self.db.lastInsertRowId();
    }
    
    /// Save a basic block
    pub fn saveBasicBlock(self: *Project, block: BasicBlock) !i64 {
        var stmt = try self.db.prepare(
            \\INSERT OR REPLACE INTO basic_blocks (address, function_id, size, instruction_count)
            \\VALUES (?, ?, ?, ?)
        );
        defer stmt.finalize();
        
        try stmt.bindInt(1, @intCast(block.address));
        
        if (block.function_id) |fid| {
            try stmt.bindInt(2, fid);
        } else {
            try stmt.bindNull(2);
        }
        
        try stmt.bindInt(3, @intCast(block.size));
        try stmt.bindInt(4, @intCast(block.instruction_count));
        
        _ = try stmt.step();
        return self.db.lastInsertRowId();
    }
    
    /// Save a CFG edge
    pub fn saveCfgEdge(self: *Project, from_block: i64, to_block: i64, edge_type: []const u8) !void {
        var stmt = try self.db.prepare(
            \\INSERT INTO cfg_edges (from_block, to_block, edge_type)
            \\VALUES (?, ?, ?)
        );
        defer stmt.finalize();
        
        try stmt.bindInt(1, from_block);
        try stmt.bindInt(2, to_block);
        try stmt.bindText(3, edge_type);
        
        _ = try stmt.step();
    }
    
    /// Save a cross-reference
    pub fn saveXref(self: *Project, from: u32, to: u32, xref_type: []const u8) !void {
        var stmt = try self.db.prepare(
            \\INSERT INTO xrefs (from_address, to_address, xref_type)
            \\VALUES (?, ?, ?)
        );
        defer stmt.finalize();
        
        try stmt.bindInt(1, @intCast(from));
        try stmt.bindInt(2, @intCast(to));
        try stmt.bindText(3, xref_type);
        
        _ = try stmt.step();
    }
    
    /// Save variable type information
    pub fn saveVariable(self: *Project, func_id: i64, var_id: u64, name: ?[]const u8, vb_type: type_lattice.VBType) !void {
        var stmt = try self.db.prepare(
            \\INSERT OR REPLACE INTO variables (function_id, var_id, name, inferred_type)
            \\VALUES (?, ?, ?, ?)
        );
        defer stmt.finalize();
        
        try stmt.bindInt(1, func_id);
        try stmt.bindInt(2, @intCast(var_id));
        
        if (name) |n| {
            try stmt.bindText(3, n);
        } else {
            try stmt.bindNull(3);
        }
        
        const type_str = type_lattice.TypeLattice.typeToString(vb_type);
        try stmt.bindText(4, type_str);
        
        _ = try stmt.step();
    }
    
    /// Save decompiled code
    pub fn saveDecompiledCode(self: *Project, func_id: i64, code: []const u8) !void {
        var stmt = try self.db.prepare(
            \\INSERT OR REPLACE INTO decompiled_code (function_id, code)
            \\VALUES (?, ?)
        );
        defer stmt.finalize();
        
        try stmt.bindInt(1, func_id);
        try stmt.bindText(2, code);
        
        _ = try stmt.step();
    }
    
    /// Load all functions
    pub fn loadFunctions(self: *Project) !std.ArrayList(Function) {
        var functions = std.ArrayList(Function).empty;
        
        var stmt = try self.db.prepare("SELECT address, name, type, is_analyzed, is_decompiled FROM functions");
        defer stmt.finalize();
        
        while (try stmt.step()) {
            const func = Function{
                .address = @intCast(stmt.columnInt(0)),
                .name = if (stmt.columnText(1).len > 0) try self.allocator.dupe(u8, stmt.columnText(1)) else null,
                .func_type = if (stmt.columnText(2).len > 0) try self.allocator.dupe(u8, stmt.columnText(2)) else null,
                .is_analyzed = stmt.columnInt(3) != 0,
                .is_decompiled = stmt.columnInt(4) != 0,
                .signature = null,
                .return_type = null,
            };
            try functions.append(self.allocator, func);
        }
        
        return functions;
    }
};

/// Initialize database schema
fn initializeSchema(db: *sqlite.Database) !void {
    // Create tables
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS project_info (
        \\    key TEXT PRIMARY KEY,
        \\    value TEXT NOT NULL
        \\)
    );
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS binary_info (
        \\    id INTEGER PRIMARY KEY,
        \\    file_path TEXT NOT NULL,
        \\    file_hash TEXT NOT NULL,
        \\    image_base INTEGER NOT NULL,
        \\    entry_point INTEGER NOT NULL,
        \\    is_vb6 BOOLEAN NOT NULL,
        \\    vb_version TEXT,
        \\    compilation_type TEXT,
        \\    binary_type TEXT,
        \\    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        \\)
    );
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS functions (
        \\    id INTEGER PRIMARY KEY,
        \\    address INTEGER NOT NULL UNIQUE,
        \\    name TEXT,
        \\    type TEXT,
        \\    is_analyzed BOOLEAN DEFAULT 0,
        \\    is_decompiled BOOLEAN DEFAULT 0,
        \\    signature TEXT,
        \\    return_type TEXT,
        \\    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        \\    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        \\)
    );
    
    try db.exec("CREATE INDEX IF NOT EXISTS idx_functions_address ON functions(address)");
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS basic_blocks (
        \\    id INTEGER PRIMARY KEY,
        \\    address INTEGER NOT NULL,
        \\    function_id INTEGER,
        \\    size INTEGER NOT NULL,
        \\    instruction_count INTEGER,
        \\    FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
        \\)
    );
    
    try db.exec("CREATE INDEX IF NOT EXISTS idx_blocks_address ON basic_blocks(address)");
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS cfg_edges (
        \\    id INTEGER PRIMARY KEY,
        \\    from_block INTEGER NOT NULL,
        \\    to_block INTEGER NOT NULL,
        \\    edge_type TEXT,
        \\    FOREIGN KEY (from_block) REFERENCES basic_blocks(id) ON DELETE CASCADE,
        \\    FOREIGN KEY (to_block) REFERENCES basic_blocks(id) ON DELETE CASCADE
        \\)
    );
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS variables (
        \\    id INTEGER PRIMARY KEY,
        \\    function_id INTEGER NOT NULL,
        \\    var_id INTEGER NOT NULL,
        \\    name TEXT,
        \\    inferred_type TEXT,
        \\    FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
        \\)
    );
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS xrefs (
        \\    id INTEGER PRIMARY KEY,
        \\    from_address INTEGER NOT NULL,
        \\    to_address INTEGER NOT NULL,
        \\    xref_type TEXT,
        \\    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        \\)
    );
    
    try db.exec("CREATE INDEX IF NOT EXISTS idx_xrefs_from ON xrefs(from_address)");
    try db.exec("CREATE INDEX IF NOT EXISTS idx_xrefs_to ON xrefs(to_address)");
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS decompiled_code (
        \\    id INTEGER PRIMARY KEY,
        \\    function_id INTEGER NOT NULL UNIQUE,
        \\    code TEXT NOT NULL,
        \\    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        \\    FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
        \\)
    );
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS comments (
        \\    id INTEGER PRIMARY KEY,
        \\    address INTEGER NOT NULL,
        \\    comment TEXT NOT NULL,
        \\    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        \\    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        \\)
    );
    
    try db.exec(
        \\CREATE TABLE IF NOT EXISTS labels (
        \\    id INTEGER PRIMARY KEY,
        \\    address INTEGER NOT NULL UNIQUE,
        \\    label TEXT NOT NULL,
        \\    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        \\)
    );
    
    // Insert default project info
    try db.exec(
        \\INSERT OR IGNORE INTO project_info (key, value) VALUES 
        \\    ('schema_version', '1'),
        \\    ('vbdecompiler_version', '0.1.0')
    );
}

/// Binary information structure
pub const BinaryInfo = struct {
    file_path: []const u8,
    file_hash: []const u8,
    image_base: u32,
    entry_point: u32,
    is_vb6: bool,
    vb_version: ?[]const u8,
    compilation_type: ?[]const u8,
    binary_type: ?[]const u8,
};

/// Function structure for database storage
pub const Function = struct {
    address: u32,
    name: ?[]const u8,
    func_type: ?[]const u8,
    is_analyzed: bool,
    is_decompiled: bool,
    signature: ?[]const u8,
    return_type: ?[]const u8,
};

/// Basic block structure for database storage
pub const BasicBlock = struct {
    address: u32,
    function_id: ?i64,
    size: u32,
    instruction_count: u32,
};
