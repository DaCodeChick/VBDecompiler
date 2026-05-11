// Dominator tree computation
// A node D dominates node N if every path from entry to N goes through D
// Used for SSA construction and various optimizations

const std = @import("std");
const cfg_mod = @import("cfg.zig");

const CFG = cfg_mod.CFG;
const BasicBlock = cfg_mod.BasicBlock;

/// Dominator tree node
pub const DomNode = struct {
    block_addr: u32, // Address of the basic block
    idom: ?u32, // Immediate dominator (null for entry block)
    dominated: std.ArrayList(u32), // Blocks directly dominated by this block
    dom_level: usize, // Distance from entry in dominator tree

    pub fn deinit(self: *DomNode, allocator: std.mem.Allocator) void {
        self.dominated.deinit(allocator);
    }
};

/// Dominator tree
pub const DominatorTree = struct {
    nodes: std.AutoHashMap(u32, DomNode),
    entry: u32, // Entry block address
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, entry: u32) DominatorTree {
        return .{
            .nodes = std.AutoHashMap(u32, DomNode).init(allocator),
            .entry = entry,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *DominatorTree) void {
        var it = self.nodes.valueIterator();
        while (it.next()) |node| {
            node.deinit(self.allocator);
        }
        self.nodes.deinit();
    }

    /// Check if block A dominates block B
    pub fn dominates(self: *const DominatorTree, a: u32, b: u32) bool {
        if (a == b) return true;

        // Walk up the dominator tree from B
        var current = b;
        while (self.nodes.get(current)) |node| {
            if (node.idom) |idom| {
                if (idom == a) return true;
                current = idom;
            } else {
                break;
            }
        }
        return false;
    }

    /// Check if block A strictly dominates block B (A dominates B and A != B)
    pub fn strictlyDominates(self: *const DominatorTree, a: u32, b: u32) bool {
        return a != b and self.dominates(a, b);
    }

    /// Get the immediate dominator of a block
    pub fn getImmediateDominator(self: *const DominatorTree, block: u32) ?u32 {
        if (self.nodes.get(block)) |node| {
            return node.idom;
        }
        return null;
    }

    /// Get all blocks dominated by the given block
    pub fn getDominated(self: *const DominatorTree, block: u32) ?[]const u32 {
        if (self.nodes.get(block)) |node| {
            return node.dominated.items;
        }
        return null;
    }
};

/// Dominance frontier - set of blocks where dominance of X ends
/// Needed for phi node placement in SSA construction
pub const DominanceFrontier = struct {
    frontiers: std.AutoHashMap(u32, std.ArrayList(u32)),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) DominanceFrontier {
        return .{
            .frontiers = std.AutoHashMap(u32, std.ArrayList(u32)).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *DominanceFrontier) void {
        var it = self.frontiers.valueIterator();
        while (it.next()) |frontier| {
            frontier.deinit(self.allocator);
        }
        self.frontiers.deinit();
    }

    /// Get the dominance frontier for a block
    pub fn getFrontier(self: *const DominanceFrontier, block: u32) ?[]const u32 {
        if (self.frontiers.get(block)) |frontier| {
            return frontier.items;
        }
        return null;
    }

    /// Add a block to another block's dominance frontier
    pub fn addToFrontier(self: *DominanceFrontier, block: u32, frontier_block: u32) !void {
        const result = try self.frontiers.getOrPut(block);
        if (!result.found_existing) {
            result.value_ptr.* = .empty;
        }

        // Check if already in frontier
        for (result.value_ptr.items) |existing| {
            if (existing == frontier_block) return;
        }

        try result.value_ptr.append(self.allocator, frontier_block);
    }
};

/// Dominator tree builder using the Lengauer-Tarjan algorithm
pub const DominatorTreeBuilder = struct {
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    entry: u32,

    pub fn init(allocator: std.mem.Allocator, cfg: *const CFG, entry: u32) DominatorTreeBuilder {
        return .{
            .allocator = allocator,
            .cfg = cfg,
            .entry = entry,
        };
    }

    /// Build the dominator tree
    pub fn build(self: *DominatorTreeBuilder) !DominatorTree {
        var tree = DominatorTree.init(self.allocator, self.entry);
        errdefer tree.deinit();

        // Get all blocks in reverse postorder (good for dominator computation)
        var blocks = try self.getReversePostorder();
        defer blocks.deinit(self.allocator);

        if (blocks.items.len == 0) {
            return tree;
        }

        // Initialize dominator sets
        // dom[entry] = {entry}
        // dom[n] = all blocks for n != entry
        var doms = std.AutoHashMap(u32, std.AutoHashMap(u32, void)).init(self.allocator);
        defer {
            var it = doms.valueIterator();
            while (it.next()) |dom_set| {
                dom_set.deinit();
            }
            doms.deinit();
        }

        // Initialize: entry dominates only itself, others dominated by all
        for (blocks.items) |block_addr| {
            var dom_set = std.AutoHashMap(u32, void).init(self.allocator);
            if (block_addr == self.entry) {
                try dom_set.put(self.entry, {});
            } else {
                // Initialize with all blocks
                for (blocks.items) |b| {
                    try dom_set.put(b, {});
                }
            }
            try doms.put(block_addr, dom_set);
        }

        // Iterative fixed-point computation
        var changed = true;
        var iteration: usize = 0;
        const max_iterations: usize = 1000;

        while (changed and iteration < max_iterations) : (iteration += 1) {
            changed = false;

            for (blocks.items) |block_addr| {
                if (block_addr == self.entry) continue;

                const block = self.cfg.blocks.get(block_addr) orelse continue;

                // New dom set = {block} ∪ (intersection of dom sets of all predecessors)
                var new_dom = std.AutoHashMap(u32, void).init(self.allocator);
                defer new_dom.deinit();

                try new_dom.put(block_addr, {}); // Block dominates itself

                if (block.predecessors.len > 0) {
                    // Start with first predecessor's dom set
                    if (doms.get(block.predecessors[0])) |first_pred_dom| {
                        var it = first_pred_dom.keyIterator();
                        while (it.next()) |key| {
                            try new_dom.put(key.*, {});
                        }
                    }

                    // Intersect with other predecessors
                    for (block.predecessors[1..]) |pred_addr| {
                        if (doms.get(pred_addr)) |pred_dom| {
                            // Keep only nodes that are in both sets
                            var to_remove: std.ArrayList(u32) = .empty;
                            defer to_remove.deinit(self.allocator);

                            var dom_it = new_dom.keyIterator();
                            while (dom_it.next()) |key| {
                                if (!pred_dom.contains(key.*)) {
                                    try to_remove.append(self.allocator, key.*);
                                }
                            }

                            for (to_remove.items) |key| {
                                _ = new_dom.remove(key);
                            }
                        }
                    }

                    // Re-add the block itself (intersection might have removed it)
                    try new_dom.put(block_addr, {});
                }

                // Check if changed
                const old_dom = doms.getPtr(block_addr).?;
                if (new_dom.count() != old_dom.count()) {
                    changed = true;
                }

                // Update
                old_dom.deinit();
                try doms.put(block_addr, try new_dom.clone());
            }
        }

        // Extract immediate dominators from dominator sets
        for (blocks.items) |block_addr| {
            if (block_addr == self.entry) {
                try tree.nodes.put(block_addr, .{
                    .block_addr = block_addr,
                    .idom = null,
                    .dominated = .empty,
                    .dom_level = 0,
                });
                continue;
            }

            const dom_set = doms.get(block_addr) orelse continue;

            // Find immediate dominator: dominator with largest dom_level (closest to block)
            var idom: ?u32 = null;

            var it = dom_set.keyIterator();
            while (it.next()) |dom_addr| {
                if (dom_addr.* == block_addr) continue; // Skip self

                // For now, use a simple heuristic
                if (idom == null) {
                    idom = dom_addr.*;
                } else {
                    // Prefer the dominator that is dominated by the current idom
                    if (dom_set.contains(idom.?)) {
                        idom = dom_addr.*;
                    }
                }
            }

            try tree.nodes.put(block_addr, .{
                .block_addr = block_addr,
                .idom = idom,
                .dominated = .empty,
                .dom_level = 0, // Will compute in second pass
            });
        }

        // Build dominated lists and compute levels
        try self.buildDominatedLists(&tree);

        return tree;
    }

    /// Build dominance frontier
    pub fn buildDominanceFrontier(self: *DominatorTreeBuilder, tree: *const DominatorTree) !DominanceFrontier {
        var frontier = DominanceFrontier.init(self.allocator);
        errdefer frontier.deinit();

        // For each block B
        var block_it = self.cfg.blocks.iterator();
        while (block_it.next()) |entry| {
            const block_addr = entry.key_ptr.*;
            const block = entry.value_ptr;

            // If B has multiple predecessors
            if (block.predecessors.len >= 2) {
                for (block.predecessors) |pred_addr| {
                    var runner = pred_addr;

                    // Walk up dominator tree until we reach B's dominator
                    while (runner != block_addr) {
                        // Add B to runner's frontier
                        try frontier.addToFrontier(runner, block_addr);

                        // Move to immediate dominator
                        if (tree.getImmediateDominator(runner)) |idom| {
                            if (idom == runner) break; // Safety check
                            runner = idom;
                        } else {
                            break;
                        }

                        // If runner dominates B, stop
                        if (tree.dominates(runner, block_addr)) {
                            break;
                        }
                    }
                }
            }
        }

        return frontier;
    }

    fn buildDominatedLists(self: *DominatorTreeBuilder, tree: *DominatorTree) !void {
        // Build dominated lists
        var node_it = tree.nodes.iterator();
        while (node_it.next()) |entry| {
            const node = entry.value_ptr;
            if (node.idom) |idom_addr| {
                if (tree.nodes.getPtr(idom_addr)) |idom_node| {
                    try idom_node.dominated.append(self.allocator, node.block_addr);
                }
            }
        }

        // Compute levels via DFS
        try self.computeLevels(tree, self.entry, 0);
    }

    fn computeLevels(self: *DominatorTreeBuilder, tree: *DominatorTree, block: u32, level: usize) !void {
        if (tree.nodes.getPtr(block)) |node| {
            node.dom_level = level;

            for (node.dominated.items) |dominated_block| {
                try self.computeLevels(tree, dominated_block, level + 1);
            }
        }
    }

    fn getReversePostorder(self: *DominatorTreeBuilder) !std.ArrayList(u32) {
        var result: std.ArrayList(u32) = .empty;
        var visited = std.AutoHashMap(u32, void).init(self.allocator);
        defer visited.deinit();

        try self.dfsPostorder(&result, &visited, self.entry);

        // Reverse the list
        std.mem.reverse(u32, result.items);

        return result;
    }

    fn dfsPostorder(self: *DominatorTreeBuilder, result: *std.ArrayList(u32), visited: *std.AutoHashMap(u32, void), block: u32) !void {
        if (visited.contains(block)) return;
        try visited.put(block, {});

        if (self.cfg.blocks.get(block)) |bb| {
            for (bb.successors) |edge| {
                try self.dfsPostorder(result, visited, edge.to);
            }
        }

        try result.append(self.allocator, block);
    }
};

test "dominator tree simple" {
    const allocator = std.testing.allocator;

    // Create a simple CFG: entry -> block1
    var test_cfg = CFG.init(allocator);
    defer test_cfg.deinit();

    const entry: u32 = 0x1000;
    const block1: u32 = 0x1010;

    const entry_bb = BasicBlock{
        .start_address = entry,
        .end_address = entry + 10,
        .instructions = &.{},
        .successors = &[_]cfg_mod.Edge{.{
            .target = block1,
            .type = .unconditional_jump,
        }},
        .predecessors = &.{},
    };

    const block1_bb = BasicBlock{
        .start_address = block1,
        .end_address = block1 + 10,
        .instructions = &.{},
        .successors = &.{},
        .predecessors = &[_]u32{entry},
    };

    try test_cfg.blocks.put(entry, entry_bb);
    try test_cfg.blocks.put(block1, block1_bb);

    // Build dominator tree
    var builder = DominatorTreeBuilder.init(allocator, &test_cfg, entry);
    var tree = try builder.build();
    defer tree.deinit();

    // Entry should dominate block1
    try std.testing.expect(tree.dominates(entry, block1));
    try std.testing.expect(tree.dominates(entry, entry));
    try std.testing.expect(!tree.dominates(block1, entry));
}
