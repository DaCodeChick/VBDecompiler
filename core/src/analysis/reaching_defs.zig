// Reaching definitions analysis
// Computes which definitions may reach each program point

const std = @import("std");
const dataflow = @import("dataflow.zig");
const pcode = @import("../ir/pcode_ops.zig");
const pcode_func = @import("../ir/pcode_function.zig");
const cfg_mod = @import("cfg.zig");

const PCodeOp = pcode.PCodeOp;
const Varnode = pcode.Varnode;
const PCodeFunction = pcode_func.PCodeFunction;
const PCodeBlock = pcode_func.PCodeBlock;
const CFG = cfg_mod.CFG;
const BasicBlock = cfg_mod.BasicBlock;
const EdgeType = cfg_mod.EdgeType;

const ReachingDefs = dataflow.ReachingDefs;
const Definition = dataflow.Definition;
const Use = dataflow.Use;
const UseDefChain = dataflow.UseDefChain;
const DefUseChain = dataflow.DefUseChain;

/// Reaching definitions analysis result
pub const ReachingDefsAnalysis = struct {
    /// Reaching definitions at the entry of each block
    block_in: std.AutoHashMap(u32, ReachingDefs),
    /// Reaching definitions at the exit of each block
    block_out: std.AutoHashMap(u32, ReachingDefs),
    /// Use-def chains
    use_def: UseDefChain,
    /// Def-use chains
    def_use: DefUseChain,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) ReachingDefsAnalysis {
        return .{
            .block_in = std.AutoHashMap(u32, ReachingDefs).init(allocator),
            .block_out = std.AutoHashMap(u32, ReachingDefs).init(allocator),
            .use_def = UseDefChain.init(allocator),
            .def_use = DefUseChain.init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *ReachingDefsAnalysis) void {
        var in_it = self.block_in.valueIterator();
        while (in_it.next()) |reach| {
            reach.deinit();
        }
        self.block_in.deinit();

        var out_it = self.block_out.valueIterator();
        while (out_it.next()) |reach| {
            reach.deinit();
        }
        self.block_out.deinit();

        self.use_def.deinit();
        self.def_use.deinit();
    }
};

/// Reaching definitions analyzer
pub const ReachingDefsAnalyzer = struct {
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    pcode_func: *const PCodeFunction,

    pub fn init(allocator: std.mem.Allocator, cfg: *const CFG, func: *const PCodeFunction) ReachingDefsAnalyzer {
        return .{
            .allocator = allocator,
            .cfg = cfg,
            .pcode_func = func,
        };
    }

    /// Run reaching definitions analysis on the function
    pub fn analyze(self: *ReachingDefsAnalyzer) !ReachingDefsAnalysis {
        var result = ReachingDefsAnalysis.init(self.allocator);
        errdefer result.deinit();

        // Initialize IN and OUT sets for all blocks
        var block_addrs: std.ArrayList(u32) = .empty;
        defer block_addrs.deinit(self.allocator);

        var it = self.pcode_func.blocks.keyIterator();
        while (it.next()) |addr| {
            try block_addrs.append(self.allocator, addr.*);
            try result.block_in.put(addr.*, ReachingDefs.init(self.allocator));
            try result.block_out.put(addr.*, ReachingDefs.init(self.allocator));
        }

        // Iterative dataflow analysis (fixed-point computation)
        var changed = true;
        var iteration: usize = 0;
        const max_iterations: usize = 1000; // Safety limit

        while (changed and iteration < max_iterations) : (iteration += 1) {
            changed = false;

            for (block_addrs.items) |block_addr| {
                const pcode_block = self.pcode_func.blocks.get(block_addr) orelse continue;
                const cfg_block = self.cfg.blocks.get(block_addr) orelse continue;

                // IN[B] = union of OUT[P] for all predecessors P of B
                var in_set = result.block_in.getPtr(block_addr).?;
                const old_in_count = in_set.defs.count();

                // Merge from all predecessors
                for (cfg_block.predecessors) |pred_addr| {
                    if (result.block_out.get(pred_addr)) |pred_out| {
                        try in_set.merge(&pred_out);
                    }
                }

                // Check if IN set changed
                if (in_set.defs.count() != old_in_count) {
                    changed = true;
                }

                // OUT[B] = GEN[B] ∪ (IN[B] - KILL[B])
                // Process each operation in the block
                var out_set = try in_set.clone();
                defer {
                    var old_out = result.block_out.get(block_addr).?;
                    old_out.deinit();
                }

                for (pcode_block.ops.items, 0..) |*op, op_idx| {
                    // Process uses (inputs) - these read from reaching definitions
                    if (op.input0) |input0| {
                        if (!isConstant(input0)) {
                            const use = Use{
                                .block_addr = block_addr,
                                .op_index = op_idx,
                                .varnode = input0,
                                .operand_index = 0,
                            };

                            // Link this use to its reaching definitions
                            if (out_set.getDefs(input0)) |defs| {
                                for (defs) |def| {
                                    try result.use_def.addLink(use, def);
                                    try result.def_use.addLink(def, use);
                                }
                            }
                        }
                    }

                    if (op.input1) |input1| {
                        if (!isConstant(input1)) {
                            const use = Use{
                                .block_addr = block_addr,
                                .op_index = op_idx,
                                .varnode = input1,
                                .operand_index = 1,
                            };

                            if (out_set.getDefs(input1)) |defs| {
                                for (defs) |def| {
                                    try result.use_def.addLink(use, def);
                                    try result.def_use.addLink(def, use);
                                }
                            }
                        }
                    }

                    // Process definition (output) - this kills previous definitions and generates a new one
                    if (op.output) |output| {
                        if (!isConstant(output)) {
                            // Kill previous definitions of this varnode
                            out_set.killDef(output);

                            // Generate new definition
                            const def = Definition{
                                .block_addr = block_addr,
                                .op_index = op_idx,
                                .varnode = output,
                            };
                            try out_set.addDef(output, def);
                        }
                    }
                }

                try result.block_out.put(block_addr, out_set);
            }
        }

        if (iteration >= max_iterations) {
            std.debug.print("Warning: Reaching definitions analysis hit iteration limit\n", .{});
        }

        return result;
    }

    /// Check if a varnode is a constant (constants don't have definitions)
    fn isConstant(vn: Varnode) bool {
        return vn.space == .constant;
    }
};

test "reaching definitions simple" {
    const allocator = std.testing.allocator;

    // Create a simple CFG with one block
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

    // Create a simple P-code function with one block
    var test_func = PCodeFunction.init(allocator, block_addr, "test");
    defer test_func.deinit();

    var test_block = try test_func.getOrCreateBlock(block_addr);
    test_block.end_address = block_addr + 10;

    // Add some P-code operations: r0 = 42, r1 = r0 + 1
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

    // Run reaching definitions analysis
    var analyzer = ReachingDefsAnalyzer.init(allocator, &test_cfg, &test_func);
    var analysis = try analyzer.analyze();
    defer analysis.deinit();

    // Check that we have results
    try std.testing.expect(analysis.block_in.count() > 0);
    try std.testing.expect(analysis.block_out.count() > 0);
}
