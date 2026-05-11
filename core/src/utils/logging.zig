// Logging utilities
const std = @import("std");

pub const LogLevel = enum {
    debug,
    info,
    warn,
    err,
};

var current_level: LogLevel = .info;

pub fn setLevel(level: LogLevel) void {
    current_level = level;
}

pub fn debug(comptime fmt: []const u8, args: anytype) void {
    if (@intFromEnum(current_level) <= @intFromEnum(LogLevel.debug)) {
        std.debug.print("[DEBUG] " ++ fmt ++ "\n", args);
    }
}

pub fn info(comptime fmt: []const u8, args: anytype) void {
    if (@intFromEnum(current_level) <= @intFromEnum(LogLevel.info)) {
        std.debug.print("[INFO] " ++ fmt ++ "\n", args);
    }
}

pub fn warn(comptime fmt: []const u8, args: anytype) void {
    if (@intFromEnum(current_level) <= @intFromEnum(LogLevel.warn)) {
        std.debug.print("[WARN] " ++ fmt ++ "\n", args);
    }
}

pub fn err(comptime fmt: []const u8, args: anytype) void {
    if (@intFromEnum(current_level) <= @intFromEnum(LogLevel.err)) {
        std.debug.print("[ERROR] " ++ fmt ++ "\n", args);
    }
}
