// Data flow analysis pretty printer
// Displays reaching definitions, liveness, and dominator information

const std = @import("std");
const dataflow = @import("dataflow.zig");
const reaching_defs = @import("reaching_defs.zig");
const liveness = @import("liveness.zig");
const dominators = @import("dominators.zig");
const pcode_func = @import("../ir/pcode_function.zig");

const ReachingDefsAnalysis = reaching_defs.ReachingDefsAnalysis;
const LivenessAnalysis = liveness.LivenessAnalysis;
const DominatorTree = dominators.DominatorTree;
const DominanceFrontier = dominators.DominanceFrontier;
const PCodeFunction = pcode_func.PCodeFunction;

/// Pretty printer options
pub const PrinterOptions = struct {
    color: bool = false,
    show_reaching_defs: bool = true,
    show_liveness: bool = true,
    show_dominators: bool = true,
    show_use_def_chains: bool = true,
};

/// Data flow analysis pretty printer
pub const DataFlowPrinter = struct {
    allocator: std.mem.Allocator,
    options: PrinterOptions,

    pub fn init(allocator: std.mem.Allocator, options: PrinterOptions) DataFlowPrinter {
        return .{
            .allocator = allocator,
            .options = options,
        };
    }

    /// Print reaching definitions analysis
    pub fn printReachingDefs(self: *const DataFlowPrinter, writer: anytype, analysis: *const ReachingDefsAnalysis, func: *const PCodeFunction) !void {
        if (!self.options.show_reaching_defs) return;

        if (self.options.color) {
            try writer.writeAll("\x1b[1;36m"); // Bold cyan
        }
        try writer.writeAll("=== Reaching Definitions ===\n");
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        // Get sorted block addresses
        var block_addrs: std.ArrayList(u32) = .empty;
        defer block_addrs.deinit(self.allocator);

        var it = func.blocks.keyIterator();
        while (it.next()) |addr| {
            try block_addrs.append(self.allocator, addr.*);
        }
        std.mem.sort(u32, block_addrs.items, {}, comptime std.sort.asc(u32));

        for (block_addrs.items) |block_addr| {
            if (self.options.color) {
                try writer.writeAll("\x1b[1;33m"); // Bold yellow
            }
            try writer.print("\nBlock 0x{x:0>8}:\n", .{block_addr});
            if (self.options.color) {
                try writer.writeAll("\x1b[0m");
            }

            // Print IN set
            if (analysis.block_in.get(block_addr)) |in_set| {
                try writer.writeAll("  IN:  ");
                var in_it = in_set.defs.iterator();
                var count: usize = 0;
                while (in_it.next()) |entry| {
                    const defs = entry.value_ptr.items;
                    if (defs.len > 0) {
                        if (count > 0) try writer.writeAll(", ");
                        try writer.print("{}:[", .{defs[0].varnode});
                        for (defs, 0..) |def, i| {
                            if (i > 0) try writer.writeAll(",");
                            try writer.print("0x{x}:{}", .{ def.block_addr, def.op_index });
                        }
                        try writer.writeAll("]");
                        count += 1;
                    }
                }
                if (count == 0) try writer.writeAll("∅");
                try writer.writeAll("\n");
            }

            // Print OUT set
            if (analysis.block_out.get(block_addr)) |out_set| {
                try writer.writeAll("  OUT: ");
                var out_it = out_set.defs.iterator();
                var count: usize = 0;
                while (out_it.next()) |entry| {
                    const defs = entry.value_ptr.items;
                    if (defs.len > 0) {
                        if (count > 0) try writer.writeAll(", ");
                        try writer.print("{}:[", .{defs[0].varnode});
                        for (defs, 0..) |def, i| {
                            if (i > 0) try writer.writeAll(",");
                            try writer.print("0x{x}:{}", .{ def.block_addr, def.op_index });
                        }
                        try writer.writeAll("]");
                        count += 1;
                    }
                }
                if (count == 0) try writer.writeAll("∅");
                try writer.writeAll("\n");
            }
        }
        try writer.writeAll("\n");
    }

    /// Print liveness analysis
    pub fn printLiveness(self: *const DataFlowPrinter, writer: anytype, analysis: *const LivenessAnalysis, func: *const PCodeFunction) !void {
        if (!self.options.show_liveness) return;

        if (self.options.color) {
            try writer.writeAll("\x1b[1;35m"); // Bold magenta
        }
        try writer.writeAll("=== Liveness Analysis ===\n");
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        // Get sorted block addresses
        var block_addrs: std.ArrayList(u32) = .empty;
        defer block_addrs.deinit(self.allocator);

        var it = func.blocks.keyIterator();
        while (it.next()) |addr| {
            try block_addrs.append(self.allocator, addr.*);
        }
        std.mem.sort(u32, block_addrs.items, {}, comptime std.sort.asc(u32));

        for (block_addrs.items) |block_addr| {
            if (self.options.color) {
                try writer.writeAll("\x1b[1;33m"); // Bold yellow
            }
            try writer.print("\nBlock 0x{x:0>8}:\n", .{block_addr});
            if (self.options.color) {
                try writer.writeAll("\x1b[0m");
            }

            // Print live-in
            if (analysis.block_in.get(block_addr)) |live_in| {
                try writer.writeAll("  Live-IN:  {");
                var live_it = live_in.live.keyIterator();
                var count: usize = 0;
                while (live_it.next()) |key| {
                    if (count > 0) try writer.writeAll(", ");
                    try writer.print("{}:{}:{}", .{ key.space, key.offset, key.size });
                    count += 1;
                }
                try writer.writeAll("}\n");
            }

            // Print live-out
            if (analysis.block_out.get(block_addr)) |live_out| {
                try writer.writeAll("  Live-OUT: {");
                var live_it = live_out.live.keyIterator();
                var count: usize = 0;
                while (live_it.next()) |key| {
                    if (count > 0) try writer.writeAll(", ");
                    try writer.print("{}:{}:{}", .{ key.space, key.offset, key.size });
                    count += 1;
                }
                try writer.writeAll("}\n");
            }
        }
        try writer.writeAll("\n");
    }

    /// Print dominator tree
    pub fn printDominators(self: *const DataFlowPrinter, writer: anytype, tree: *const DominatorTree, frontier: *const DominanceFrontier) !void {
        if (!self.options.show_dominators) return;

        if (self.options.color) {
            try writer.writeAll("\x1b[1;32m"); // Bold green
        }
        try writer.writeAll("=== Dominator Tree ===\n");
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        // Get sorted block addresses
        var block_addrs: std.ArrayList(u32) = .empty;
        defer block_addrs.deinit(self.allocator);

        var it = tree.nodes.keyIterator();
        while (it.next()) |addr| {
            try block_addrs.append(self.allocator, addr.*);
        }
        std.mem.sort(u32, block_addrs.items, {}, comptime std.sort.asc(u32));

        for (block_addrs.items) |block_addr| {
            const node = tree.nodes.get(block_addr).?;

            try writer.print("Block 0x{x:0>8}: ", .{block_addr});

            if (node.idom) |idom| {
                try writer.print("idom=0x{x:0>8}", .{idom});
            } else {
                try writer.writeAll("idom=ENTRY");
            }

            if (node.dominated.items.len > 0) {
                try writer.writeAll(", dominates=[");
                for (node.dominated.items, 0..) |dominated, i| {
                    if (i > 0) try writer.writeAll(", ");
                    try writer.print("0x{x:0>8}", .{dominated});
                }
                try writer.writeAll("]");
            }

            if (frontier.getFrontier(block_addr)) |df| {
                if (df.len > 0) {
                    try writer.writeAll(", DF=[");
                    for (df, 0..) |f, i| {
                        if (i > 0) try writer.writeAll(", ");
                        try writer.print("0x{x:0>8}", .{f});
                    }
                    try writer.writeAll("]");
                }
            }

            try writer.writeAll("\n");
        }
        try writer.writeAll("\n");
    }

    /// Print use-def chains
    pub fn printUseDefChains(self: *const DataFlowPrinter, writer: anytype, analysis: *const ReachingDefsAnalysis) !void {
        if (!self.options.show_use_def_chains) return;

        if (self.options.color) {
            try writer.writeAll("\x1b[1;34m"); // Bold blue
        }
        try writer.writeAll("=== Use-Def Chains ===\n");
        if (self.options.color) {
            try writer.writeAll("\x1b[0m");
        }

        var it = analysis.use_def.chains.iterator();
        var count: usize = 0;
        while (it.next()) |entry| : (count += 1) {
            const use = entry.key_ptr.*;
            const defs = entry.value_ptr.items;

            try writer.print("USE 0x{x:0>8}[{}] {} -> ", .{ use.block_addr, use.op_index, use.varnode });
            
            if (defs.len == 0) {
                try writer.writeAll("∅");
            } else {
                try writer.writeAll("DEF ");
                for (defs, 0..) |def, i| {
                    if (i > 0) try writer.writeAll(", ");
                    try writer.print("0x{x:0>8}[{}]", .{ def.block_addr, def.op_index });
                }
            }
            try writer.writeAll("\n");

            if (count > 50) {
                try writer.writeAll("  ... (truncated)\n");
                break;
            }
        }
        try writer.writeAll("\n");
    }

    /// Print all analysis results
    pub fn printAll(
        self: *const DataFlowPrinter,
        writer: anytype,
        reach: *const ReachingDefsAnalysis,
        live: *const LivenessAnalysis,
        tree: *const DominatorTree,
        frontier: *const DominanceFrontier,
        func: *const PCodeFunction,
    ) !void {
        try self.printReachingDefs(writer, reach, func);
        try self.printLiveness(writer, live, func);
        try self.printDominators(writer, tree, frontier);
        try self.printUseDefChains(writer, reach);
    }
};
