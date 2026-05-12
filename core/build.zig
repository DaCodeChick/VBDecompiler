const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Create a module for the library
    const lib_module = b.createModule(.{
        .root_source_file = b.path("src/lib.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    // Shared library for GUI/external use
    const lib = b.addLibrary(.{
        .name = "vbdecomp",
        .root_module = lib_module,
        .linkage = .dynamic,
        .version = .{ .major = 0, .minor = 1, .patch = 0 },
    });
    
    // Link SQLite3
    lib.root_module.linkSystemLibrary("sqlite3", .{});
    
    b.installArtifact(lib);

    // Create module for executable
    const exe_module = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    // CLI executable
    const exe = b.addExecutable(.{
        .name = "vbdecomp",
        .root_module = exe_module,
    });
    
    // Link SQLite3
    exe.root_module.linkSystemLibrary("sqlite3", .{});
    
    b.installArtifact(exe);

    // Run command for CLI
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the CLI tool");
    run_step.dependOn(&run_cmd.step);

    // Unit tests
    const lib_test_module = b.createModule(.{
        .root_source_file = b.path("src/lib.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    
    const lib_unit_tests = b.addTest(.{
        .root_module = lib_test_module,
    });

    const run_lib_unit_tests = b.addRunArtifact(lib_unit_tests);

    const exe_test_module = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    
    const exe_unit_tests = b.addTest(.{
        .root_module = exe_test_module,
    });

    const run_exe_unit_tests = b.addRunArtifact(exe_unit_tests);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_lib_unit_tests.step);
    test_step.dependOn(&run_exe_unit_tests.step);
}
