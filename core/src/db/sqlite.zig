// SQLite Database wrapper for VBDecompiler
// Provides a safe Zig interface to SQLite for storing analysis results

const std = @import("std");

// Import SQLite C API
const c = @cImport({
    @cInclude("sqlite3.h");
});

pub const SqliteError = error{
    OpenFailed,
    ExecFailed,
    PrepareFailed,
    StepFailed,
    BindFailed,
    ColumnFailed,
    FinalizeFailed,
};

/// SQLite database connection
pub const Database = struct {
    db: ?*c.sqlite3,
    allocator: std.mem.Allocator,
    
    /// Open or create a database
    pub fn open(allocator: std.mem.Allocator, path: []const u8) !Database {
        var db: ?*c.sqlite3 = null;
        
        // Add null terminator for C API
        const path_z = try allocator.dupeZ(u8, path);
        defer allocator.free(path_z);
        
        const result = c.sqlite3_open(path_z.ptr, &db);
        if (result != c.SQLITE_OK) {
            if (db) |database| {
                _ = c.sqlite3_close(database);
            }
            return SqliteError.OpenFailed;
        }
        
        return Database{
            .db = db,
            .allocator = allocator,
        };
    }
    
    /// Close the database
    pub fn close(self: *Database) void {
        if (self.db) |db| {
            _ = c.sqlite3_close(db);
            self.db = null;
        }
    }
    
    /// Execute a SQL statement (no result expected)
    pub fn exec(self: *Database, sql: []const u8) !void {
        const db = self.db orelse return SqliteError.ExecFailed;
        
        const sql_z = try self.allocator.dupeZ(u8, sql);
        defer self.allocator.free(sql_z);
        
        const result = c.sqlite3_exec(db, sql_z.ptr, null, null, null);
        if (result != c.SQLITE_OK) {
            return SqliteError.ExecFailed;
        }
    }
    
    /// Prepare a SQL statement
    pub fn prepare(self: *Database, sql: []const u8) !Statement {
        const db = self.db orelse return SqliteError.PrepareFailed;
        
        const sql_z = try self.allocator.dupeZ(u8, sql);
        defer self.allocator.free(sql_z);
        
        var stmt: ?*c.sqlite3_stmt = null;
        const result = c.sqlite3_prepare_v2(
            db,
            sql_z.ptr,
            @intCast(sql.len),
            &stmt,
            null
        );
        
        if (result != c.SQLITE_OK) {
            return SqliteError.PrepareFailed;
        }
        
        return Statement{
            .stmt = stmt,
            .allocator = self.allocator,
        };
    }
    
    /// Get last insert row ID
    pub fn lastInsertRowId(self: *Database) i64 {
        const db = self.db orelse return 0;
        return c.sqlite3_last_insert_rowid(db);
    }
    
    /// Get error message
    pub fn getErrorMsg(self: *Database) []const u8 {
        const db = self.db orelse return "No database connection";
        const msg = c.sqlite3_errmsg(db);
        return std.mem.span(msg);
    }
};

/// Prepared SQL statement
pub const Statement = struct {
    stmt: ?*c.sqlite3_stmt,
    allocator: std.mem.Allocator,
    
    /// Finalize the statement
    pub fn finalize(self: *Statement) void {
        if (self.stmt) |stmt| {
            _ = c.sqlite3_finalize(stmt);
            self.stmt = null;
        }
    }
    
    /// Reset the statement for reuse
    pub fn reset(self: *Statement) !void {
        const stmt = self.stmt orelse return SqliteError.StepFailed;
        const result = c.sqlite3_reset(stmt);
        if (result != c.SQLITE_OK) {
            return SqliteError.StepFailed;
        }
    }
    
    /// Bind an integer parameter
    pub fn bindInt(self: *Statement, index: i32, value: i64) !void {
        const stmt = self.stmt orelse return SqliteError.BindFailed;
        const result = c.sqlite3_bind_int64(stmt, index, value);
        if (result != c.SQLITE_OK) {
            return SqliteError.BindFailed;
        }
    }
    
    /// Bind a text parameter
    pub fn bindText(self: *Statement, index: i32, value: []const u8) !void {
        const stmt = self.stmt orelse return SqliteError.BindFailed;
        const result = c.sqlite3_bind_text(
            stmt,
            index,
            value.ptr,
            @intCast(value.len),
            c.SQLITE_TRANSIENT
        );
        if (result != c.SQLITE_OK) {
            return SqliteError.BindFailed;
        }
    }
    
    /// Bind a null parameter
    pub fn bindNull(self: *Statement, index: i32) !void {
        const stmt = self.stmt orelse return SqliteError.BindFailed;
        const result = c.sqlite3_bind_null(stmt, index);
        if (result != c.SQLITE_OK) {
            return SqliteError.BindFailed;
        }
    }
    
    /// Execute the statement and move to next row
    pub fn step(self: *Statement) !bool {
        const stmt = self.stmt orelse return SqliteError.StepFailed;
        const result = c.sqlite3_step(stmt);
        
        if (result == c.SQLITE_ROW) {
            return true; // More rows available
        } else if (result == c.SQLITE_DONE) {
            return false; // No more rows
        } else {
            return SqliteError.StepFailed;
        }
    }
    
    /// Get integer column value
    pub fn columnInt(self: *Statement, index: i32) i64 {
        const stmt = self.stmt orelse return 0;
        return c.sqlite3_column_int64(stmt, index);
    }
    
    /// Get text column value
    pub fn columnText(self: *Statement, index: i32) []const u8 {
        const stmt = self.stmt orelse return "";
        const text = c.sqlite3_column_text(stmt, index);
        if (text == null) return "";
        const len = c.sqlite3_column_bytes(stmt, index);
        return text[0..@intCast(len)];
    }
    
    /// Get column count
    pub fn columnCount(self: *Statement) i32 {
        const stmt = self.stmt orelse return 0;
        return c.sqlite3_column_count(stmt);
    }
};

/// Transaction helper
pub const Transaction = struct {
    db: *Database,
    committed: bool,
    
    pub fn begin(db: *Database) !Transaction {
        try db.exec("BEGIN TRANSACTION");
        return Transaction{
            .db = db,
            .committed = false,
        };
    }
    
    pub fn commit(self: *Transaction) !void {
        try self.db.exec("COMMIT");
        self.committed = true;
    }
    
    pub fn rollback(self: *Transaction) !void {
        if (!self.committed) {
            try self.db.exec("ROLLBACK");
        }
    }
};
