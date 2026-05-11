// Liveness analysis
// Computes which variables are live (may be used later) at each program point

const std = @import("std");
const dataflow = @import("dataflow.zig");
const pcode = @import("../ir/pcode_ops.zig");
const pcode_func = @import("../ir/pcode_function.zig");
const cfg_mod = @import("cfg.zig");

const Varnode = pcode.Varnode;
const PCodeFunction = pcode_func.PCodeFunction;
const PCodeBlock = pcode_func.PCodeBlock;
const CFG = cfg_mod.CFG;
const BasicBlock = cfg_mod.BasicBlock;
const LiveSet = dataflow.LiveSet;
const VarnodeKey = dataflow.VarnodeKey;

/// Liveness analysis result
pub const LivenessAnalysis = struct {
    /// Live variables at the entry of each block
    block_in: std.AutoHashMap(u32, LiveSet),
    /// Live variables at the exit of each block
    block_out: std.AutoHashMap(u32, LiveSet),
    /// UEVar (upward exposed variables) - variables used before defined in block
    block_uevar: std.AutoHashMap(u32, LiveSet),
    /// VarKill - variables defined in the block
    block_varkill: std.AutoHashMap(u32, LiveSet),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) LivenessAnalysis {
        return .{
            .block_in = std.AutoHashMap(u32, LiveSet).init(allocator),
            .block_out = std.AutoHashMap(u32, LiveSet).init(allocator),
            .block_uevar = std.AutoHashMap(u32, LiveSet).init(allocator),
            .block_varkill = std.AutoHashMap(u32, LiveSet).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *LivenessAnalysis) void {
        var in_it = self.block_in.valueIterator();
        while (in_it.next()) |live| {
            live.deinit();
        }
        self.block_in.deinit();

        var out_it = self.block_out.valueIterator();
        while (out_it.next()) |live| {
            live.deinit();
        }
        self.block_out.deinit();

        var uevar_it = self.block_uevar.valueIterator();
        while (uevar_it.next()) |live| {
            live.deinit();
        }
        self.block_uevar.deinit();

        var varkill_it = self.block_varkill.valueIterator();
        while (varkill_it.next()) |live| {
            live.deinit();
        }
        self.block_varkill.deinit();
    }

    /// Check if a variable is live at block entry
    pub fn isLiveAtBlockEntry(self: *const LivenessAnalysis, block_addr: u32, vn: Varnode) bool {
        if (self.block_in.get(block_addr)) |live| {
            return live.contains(vn);
        }
        return false;
    }

    /// Check if a variable is live at block exit
    pub fn isLiveAtBlockExit(self: *const LivenessAnalysis, block_addr: u32, vn: Varnode) bool {
        if (self.block_out.get(block_addr)) |live| {
            return live.contains(vn);
        }
        return false;
    }
};

/// Liveness analyzer
pub const LivenessAnalyzer = struct {
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    pcode_func: *const PCodeFunction,

    pub fn init(allocator: std.mem.Allocator, cfg: *const CFG, func: *const PCodeFunction) LivenessAnalyzer {
        return .{
            .allocator = allocator,
            .cfg = cfg,
            .pcode_func = func,
        };
    }

    /// Run liveness analysis on the function
    pub fn analyze(self: *LivenessAnalyzer) !LivenessAnalysis {
        var result = LivenessAnalysis.init(self.allocator);
        errdefer result.deinit();

        // Get all block addresses
        var block_addrs: std.ArrayList(u32) = .empty;
        defer block_addrs.deinit(self.allocator);

        var it = self.pcode_func.blocks.keyIterator();
        while (it.next()) |addr| {
            try block_addrs.append(self.allocator, addr.*);
        }

        // Compute UEVar and VarKill for each block
        for (block_addrs.items) |block_addr| {
            const pcode_block = self.pcode_func.blocks.get(block_addr) orelse continue;

            var uevar = LiveSet.init(self.allocator);
            var varkill = LiveSet.init(self.allocator);

            // Process operations in order
            for (pcode_block.ops.items) |*op| {
                // Check uses before the definition kills them
                if (op.input0) |input0| {
                    if (!isConstant(input0) and !varkill.contains(input0)) {
                        try uevar.add(input0);
                    }
                }

                if (op.input1) |input1| {
                    if (!isConstant(input1) and !varkill.contains(input1)) {
                        try uevar.add(input1);
                    }
                }

                // Definition kills the variable
                if (op.output) |output| {
                    if (!isConstant(output)) {
                        try varkill.add(output);
                    }
                }
            }

            try result.block_uevar.put(block_addr, uevar);
            try result.block_varkill.put(block_addr, varkill);
        }

        // Initialize IN and OUT sets
        for (block_addrs.items) |addr| {
            try result.block_in.put(addr, LiveSet.init(self.allocator));
            try result.block_out.put(addr, LiveSet.init(self.allocator));
        }

        // Iterative dataflow analysis (backward)
        // OUT[B] = union of IN[S] for all successors S of B
        // IN[B] = UEVar[B] ∪ (OUT[B] - VarKill[B])
        var changed = true;
        var iteration: usize = 0;
        const max_iterations: usize = 1000;

        while (changed and iteration < max_iterations) : (iteration += 1) {
            changed = false;

            // Process blocks in reverse order (backward analysis)
            var i = block_addrs.items.len;
            while (i > 0) {
                i -= 1;
                const block_addr = block_addrs.items[i];
                const cfg_block = self.cfg.blocks.get(block_addr) orelse continue;

                // OUT[B] = union of IN[S] for all successors
                var out_set = result.block_out.getPtr(block_addr).?;
                const old_out_count = out_set.count();

                // Merge from all successors
                for (cfg_block.successors) |edge| {
                    if (result.block_in.get(edge.to)) |succ_in| {
                        try out_set.merge(&succ_in);
                    }
                }

                if (out_set.count() != old_out_count) {
                    changed = true;
                }

                // IN[B] = UEVar[B] ∪ (OUT[B] - VarKill[B])
                const uevar = result.block_uevar.get(block_addr).?;
                const varkill = result.block_varkill.get(block_addr).?;

                var in_set = result.block_in.getPtr(block_addr).?;
                const old_in_count = in_set.count();

                // Start with UEVar
                var new_in = try uevar.clone();
                defer {
                    in_set.deinit();
                }

                // Add OUT - VarKill
                var out_it = out_set.live.keyIterator();
                while (out_it.next()) |key| {
                    // Check if this varnode is in VarKill
                    if (!varkill.live.contains(key.*)) {
                        // Reconstruct varnode from key (approximate)
                        const vn = Varnode{
                            .space = key.space,
                            .offset = key.offset,
                            .size = key.size,
                        };
                        try new_in.add(vn);
                    }
                }

                if (new_in.count() != old_in_count) {
                    changed = true;
                }

                try result.block_in.put(block_addr, new_in);
            }
        }

        if (iteration >= max_iterations) {
            std.debug.print("Warning: Liveness analysis hit iteration limit\n", .{});
        }

        return result;
    }

    fn isConstant(vn: Varnode) bool {
        return vn.space == .constant;
    }
};

test "liveness analysis simple" {
    const allocator = std.testing.allocator;

    // Create a simple CFG
    var test_cfg = CFG.init(allocator);
    defer test_cfg.deinit();

    const block_addr: u32 = 0x1000;
    const bb = BasicBlock{
        .start_address = block_addr,
        .end_address = block_addr + 10,
        .instructions = &.{},
        .successors = &.{},
        .predecessors = &.{},
    };

    try test_cfg.blocks.put(block_addr, bb);

    // Create a P-code function
    var test_func = PCodeFunction.init(allocator, block_addr, "test");
    defer test_func.deinit();

    var test_block = try test_func.getOrCreateBlock(block_addr);
    test_block.end_address = block_addr + 10;

    // Add operations: r0 = 42, r1 = r0 + 1
    try test_block.ops.append(allocator, .{
        .opcode = .copy,
        .output = Varnode.register(0, 4),
        .input0 = Varnode.constant(42, 4),
        .input1 = null,
        .seq_num = 0,
    });

    try test_block.ops.append(allocator, .{
        .opcode = .int_add,
        .output = Varnode.register(1, 4),
        .input0 = Varnode.register(0, 4),
        .input1 = Varnode.constant(1, 4),
        .seq_num = 1,
    });

    // Run liveness analysis
    var analyzer = LivenessAnalyzer.init(allocator, &test_cfg, &test_func);
    var analysis = try analyzer.analyze();
    defer analysis.deinit();

    // Check that we have results
    try std.testing.expect(analysis.block_in.count() > 0);
    try std.testing.expect(analysis.block_out.count() > 0);
}
