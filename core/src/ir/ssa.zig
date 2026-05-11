// SSA (Static Single Assignment) form representation
// Converts P-code into SSA form where each variable is assigned exactly once
// Uses phi nodes at dominance frontiers to merge values from different paths

const std = @import("std");
const pcode_ops = @import("pcode_ops.zig");
const PCodeFunction = @import("pcode_function.zig").PCodeFunction;
const PCodeBlock = @import("pcode_function.zig").PCodeBlock;
const cfg_mod = @import("../analysis/cfg.zig");
const DominatorTree = @import("../analysis/dominators.zig").DominatorTree;
const DominanceFrontier = @import("../analysis/dominators.zig").DominanceFrontier;

const Varnode = pcode_ops.Varnode;
const PCodeOp = pcode_ops.PCodeOp;

/// SSA version of a varnode - includes version number
pub const SSAVarnode = struct {
    space: pcode_ops.AddressSpace,
    offset: u64,
    size: u32,
    version: u32, // SSA version number (0 for original)

    pub fn fromVarnode(vn: Varnode, version: u32) SSAVarnode {
        return .{
            .space = vn.space,
            .offset = vn.offset,
            .size = vn.size,
            .version = version,
        };
    }

    pub fn toVarnode(self: SSAVarnode) Varnode {
        return .{
            .space = self.space,
            .offset = self.offset,
            .size = self.size,
        };
    }

    pub fn format(self: SSAVarnode, comptime fmt: []const u8, options: std.fmt.FormatOptions, writer: anytype) !void {
        _ = fmt;
        _ = options;
        const vn = self.toVarnode();
        try vn.format("", .{}, writer);
        if (self.version > 0) {
            try writer.print("_{d}", .{self.version});
        }
    }
};

/// Phi node for merging values at dominance frontiers
pub const PhiNode = struct {
    output: SSAVarnode, // Variable being defined
    inputs: std.ArrayList(PhiInput), // Inputs from predecessors
    allocator: std.mem.Allocator,

    pub const PhiInput = struct {
        varnode: SSAVarnode,
        predecessor: u32, // Block address of predecessor
    };

    pub fn init(allocator: std.mem.Allocator, output: SSAVarnode) PhiNode {
        return .{
            .output = output,
            .inputs = .empty,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *PhiNode) void {
        self.inputs.deinit(self.allocator);
    }

    pub fn addInput(self: *PhiNode, varnode: SSAVarnode, predecessor: u32) !void {
        try self.inputs.append(self.allocator, .{
            .varnode = varnode,
            .predecessor = predecessor,
        });
    }
};

/// SSA form of a P-code instruction
pub const SSAInstruction = struct {
    opcode: pcode_ops.OpCode,
    output: ?SSAVarnode,
    input0: ?SSAVarnode,
    input1: ?SSAVarnode,
    address: u32, // Original instruction address
};

/// SSA form of a basic block
pub const SSABlock = struct {
    address: u32,
    phi_nodes: std.ArrayList(PhiNode),
    instructions: std.ArrayList(SSAInstruction),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, address: u32) SSABlock {
        return .{
            .address = address,
            .phi_nodes = .empty,
            .instructions = .empty,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *SSABlock) void {
        for (self.phi_nodes.items) |*phi| {
            phi.deinit();
        }
        self.phi_nodes.deinit(self.allocator);
        self.instructions.deinit(self.allocator);
    }

    pub fn addPhiNode(self: *SSABlock, phi: PhiNode) !void {
        try self.phi_nodes.append(self.allocator, phi);
    }

    pub fn addInstruction(self: *SSABlock, inst: SSAInstruction) !void {
        try self.instructions.append(self.allocator, inst);
    }
};

/// SSA form of a function
pub const SSAFunction = struct {
    blocks: std.AutoHashMap(u32, SSABlock),
    entry: u32,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, entry: u32) SSAFunction {
        return .{
            .blocks = std.AutoHashMap(u32, SSABlock).init(allocator),
            .entry = entry,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *SSAFunction) void {
        var it = self.blocks.valueIterator();
        while (it.next()) |block| {
            var b = block;
            b.deinit();
        }
        self.blocks.deinit();
    }

    pub fn addBlock(self: *SSAFunction, block: SSABlock) !void {
        try self.blocks.put(block.address, block);
    }

    pub fn getBlock(self: *SSAFunction, address: u32) ?*SSABlock {
        return self.blocks.getPtr(address);
    }
};

/// Variable definition site for tracking versions
const DefSite = struct {
    block: u32,
    instruction: usize, // Index in block, or max for phi node
};

/// SSA converter - transforms P-code into SSA form
pub const SSAConverter = struct {
    allocator: std.mem.Allocator,
    cfg: *const cfg_mod.CFG,
    pcode_func: *const PCodeFunction,
    dom_tree: *const DominatorTree,
    dom_frontier: *const DominanceFrontier,

    // Tracking state
    var_versions: std.AutoHashMap(Varnode, u32), // Current version for each variable
    var_stack: std.AutoHashMap(Varnode, std.ArrayList(u32)), // Stack of versions for renaming

    pub fn init(
        allocator: std.mem.Allocator,
        cfg: *const cfg_mod.CFG,
        pcode_func: *const PCodeFunction,
        dom_tree: *const DominatorTree,
        dom_frontier: *const DominanceFrontier,
    ) SSAConverter {
        return .{
            .allocator = allocator,
            .cfg = cfg,
            .pcode_func = pcode_func,
            .dom_tree = dom_tree,
            .dom_frontier = dom_frontier,
            .var_versions = std.AutoHashMap(Varnode, u32).init(allocator),
            .var_stack = std.AutoHashMap(Varnode, std.ArrayList(u32)).init(allocator),
        };
    }

    pub fn deinit(self: *SSAConverter) void {
        self.var_versions.deinit();

        var it = self.var_stack.valueIterator();
        while (it.next()) |stack| {
            var s = stack;
            s.deinit(self.allocator);
        }
        self.var_stack.deinit();
    }

    /// Convert P-code function to SSA form
    pub fn convert(self: *SSAConverter) !SSAFunction {
        var ssa_func = SSAFunction.init(self.allocator, self.pcode_func.address);
        errdefer ssa_func.deinit();

        // Step 1: Place phi nodes at dominance frontiers
        try self.placePhiNodes(&ssa_func);

        // Step 2: Rename variables to unique versions
        try self.renameVariables(&ssa_func);

        return ssa_func;
    }

    /// Place phi nodes at dominance frontiers for variables
    fn placePhiNodes(self: *SSAConverter, ssa_func: *SSAFunction) !void {
        // Collect all variables that are defined in the function
        var defined_vars = std.AutoHashMap(Varnode, void).init(self.allocator);
        defer defined_vars.deinit();

        // Find all variable definitions
        var block_it = self.pcode_func.blocks.iterator();
        while (block_it.next()) |entry| {
            const block = entry.value_ptr;
            for (block.ops.items) |op| {
                if (op.output) |output| {
                    // Only track registers and memory (not constants/uniques)
                    if (output.space == .register or output.space == .ram) {
                        try defined_vars.put(output, {});
                    }
                }
            }
        }

        // For each variable, place phi nodes at dominance frontiers
        var var_it = defined_vars.keyIterator();
        while (var_it.next()) |varnode| {
            try self.placePhiNodesForVariable(ssa_func, varnode.*);
        }
    }

    fn placePhiNodesForVariable(self: *SSAConverter, ssa_func: *SSAFunction, varnode: Varnode) !void {
        // Find all blocks that define this variable
        var def_blocks = std.AutoHashMap(u32, void).init(self.allocator);
        defer def_blocks.deinit();

        var block_it = self.pcode_func.blocks.iterator();
        while (block_it.next()) |entry| {
            const block_addr = entry.key_ptr.*;
            const block = entry.value_ptr;

            for (block.ops.items) |op| {
                if (op.output) |output| {
                    if (std.meta.eql(output, varnode)) {
                        try def_blocks.put(block_addr, {});
                        break;
                    }
                }
            }
        }

        // Compute iterated dominance frontier
        var phi_blocks = std.AutoHashMap(u32, void).init(self.allocator);
        defer phi_blocks.deinit();

        var worklist: std.ArrayList(u32) = .empty;
        defer worklist.deinit(self.allocator);

        // Initialize worklist with all definition blocks
        var def_it = def_blocks.keyIterator();
        while (def_it.next()) |block_addr| {
            try worklist.append(self.allocator, block_addr.*);
        }

        // Iterate until worklist is empty
        while (worklist.items.len > 0) {
            const block_addr_item = worklist.pop() orelse unreachable; // Safe due to while condition

            // Get dominance frontier of this block
            if (self.dom_frontier.getFrontier(block_addr_item)) |frontier| {
                for (frontier) |df_block| {
                    // If we haven't placed a phi node here yet
                    const result = try phi_blocks.getOrPut(df_block);
                    if (!result.found_existing) {
                        // Create SSA block if it doesn't exist
                        if (!ssa_func.blocks.contains(df_block)) {
                            try ssa_func.addBlock(SSABlock.init(self.allocator, df_block));
                        }

                        // Add phi node for this variable
                        const ssa_block = ssa_func.getBlock(df_block).?;
                        const ssa_varnode = SSAVarnode.fromVarnode(varnode, 0);
                        const phi = PhiNode.init(self.allocator, ssa_varnode);

                        // Phi nodes will be filled in during renaming pass
                        try ssa_block.addPhiNode(phi);

                        // Add to worklist if this is a new definition site
                        try worklist.append(self.allocator, df_block);
                    }
                }
            }
        }
    }

    /// Rename variables to SSA form with unique versions
    fn renameVariables(self: *SSAConverter, ssa_func: *SSAFunction) !void {
        // Initialize version counters and stacks
        var block_it = self.pcode_func.blocks.iterator();
        while (block_it.next()) |entry| {
            const block = entry.value_ptr;
            for (block.ops.items) |op| {
                if (op.output) |output| {
                    if (output.space == .register or output.space == .ram) {
                        try self.var_versions.put(output, 0);
                        const result = try self.var_stack.getOrPut(output);
                        if (!result.found_existing) {
                            result.value_ptr.* = .empty;
                        }
                    }
                }
            }
        }

        // Perform renaming starting from entry block
        try self.renameBlock(ssa_func, self.pcode_func.address);
    }

    fn renameBlock(self: *SSAConverter, ssa_func: *SSAFunction, block_addr: u32) !void {
        const pcode_block = self.pcode_func.blocks.get(block_addr) orelse return;

        // Get or create SSA block
        if (!ssa_func.blocks.contains(block_addr)) {
            try ssa_func.addBlock(SSABlock.init(self.allocator, block_addr));
        }
        const ssa_block = ssa_func.getBlock(block_addr).?;

        // Track versions pushed in this block (for popping later)
        var pushed_vars: std.ArrayList(Varnode) = .empty;
        defer pushed_vars.deinit(self.allocator);

        // Process phi nodes (assign new versions)
        for (ssa_block.phi_nodes.items) |*phi| {
            const varnode = phi.output.toVarnode();
            const new_version = try self.newVersion(varnode);
            phi.output.version = new_version;
            try pushed_vars.append(self.allocator, varnode);
        }

        // Process instructions
        for (pcode_block.ops.items) |op| {
            // Rename inputs
            var ssa_input0: ?SSAVarnode = null;
            if (op.input0) |input| {
                const version = self.getCurrentVersion(input);
                ssa_input0 = SSAVarnode.fromVarnode(input, version);
            }

            var ssa_input1: ?SSAVarnode = null;
            if (op.input1) |input| {
                const version = self.getCurrentVersion(input);
                ssa_input1 = SSAVarnode.fromVarnode(input, version);
            }

            // Rename output and assign new version
            var ssa_output: ?SSAVarnode = null;
            if (op.output) |output| {
                if (output.space == .register or output.space == .ram) {
                    const new_version = try self.newVersion(output);
                    ssa_output = SSAVarnode.fromVarnode(output, new_version);
                    try pushed_vars.append(self.allocator, output);
                } else {
                    ssa_output = SSAVarnode.fromVarnode(output, 0);
                }
            }

            // Create SSA instruction
            const ssa_inst = SSAInstruction{
                .opcode = op.opcode,
                .output = ssa_output,
                .input0 = ssa_input0,
                .input1 = ssa_input1,
                .address = block_addr, // Use block address as we don't have per-op addresses
            };

            try ssa_block.addInstruction(ssa_inst);
        }

        // Fill in phi nodes in successors
        const cfg_block = self.cfg.blocks.get(block_addr) orelse return;
        for (cfg_block.successors) |edge| {
            if (ssa_func.getBlock(edge.to)) |succ_block| {
                for (succ_block.phi_nodes.items) |*phi| {
                    const varnode = phi.output.toVarnode();
                    const version = self.getCurrentVersion(varnode);
                    try phi.addInput(SSAVarnode.fromVarnode(varnode, version), block_addr);
                }
            }
        }

        // Recursively rename dominated blocks
        if (self.dom_tree.getDominated(block_addr)) |dominated| {
            for (dominated) |dom_block| {
                if (dom_block != block_addr) { // Don't recurse to self
                    try self.renameBlock(ssa_func, dom_block);
                }
            }
        }

        // Pop versions for this block
        for (pushed_vars.items) |varnode| {
            if (self.var_stack.getPtr(varnode)) |stack| {
                _ = stack.pop();
            }
        }
    }

    fn newVersion(self: *SSAConverter, varnode: Varnode) !u32 {
        const version_ptr = self.var_versions.getPtr(varnode) orelse {
            try self.var_versions.put(varnode, 1);
            return 0;
        };
        version_ptr.* += 1;
        const new_version = version_ptr.*;

        // Push onto stack
        const stack_result = try self.var_stack.getOrPut(varnode);
        if (!stack_result.found_existing) {
            stack_result.value_ptr.* = .empty;
        }
        try stack_result.value_ptr.append(self.allocator, new_version);

        return new_version;
    }

    fn getCurrentVersion(self: *SSAConverter, varnode: Varnode) u32 {
        if (varnode.space != .register and varnode.space != .ram) {
            return 0; // Constants, uniques, etc. have no versions
        }

        if (self.var_stack.get(varnode)) |stack| {
            if (stack.items.len > 0) {
                return stack.items[stack.items.len - 1];
            }
        }
        return 0; // Uninitialized variable
    }
};
