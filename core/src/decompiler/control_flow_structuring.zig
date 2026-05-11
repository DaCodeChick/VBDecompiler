// Control Flow Structuring - Converts CFG to structured control flow
// Uses interval analysis and pattern matching to identify loops and conditionals

const std = @import("std");
const CFG = @import("../analysis/cfg.zig").CFG;
const BasicBlock = @import("../analysis/cfg.zig").BasicBlock;
const Edge = @import("../analysis/cfg.zig").Edge;
const EdgeType = @import("../analysis/cfg.zig").EdgeType;
const DominatorTree = @import("../analysis/dominators.zig").DominatorTree;
const vb6_ast = @import("vb6_ast.zig");

/// Control flow region types
pub const RegionType = enum {
    sequence,      // Linear sequence of blocks
    if_then,       // If-Then
    if_then_else,  // If-Then-Else
    while_loop,    // While loop
    do_while_loop, // Do-While loop
    for_loop,      // For loop (detected pattern)
    switch,        // Switch/Select Case
};

/// A structured control flow region
pub const Region = struct {
    type: RegionType,
    entry: u32,
    exit: u32,
    blocks: std.ArrayList(u32),
    children: std.ArrayList(*Region),
    allocator: std.mem.Allocator,
    
    pub fn init(allocator: std.mem.Allocator, region_type: RegionType, entry: u32) *Region {
        const region = allocator.create(Region) catch unreachable;
        region.* = .{
            .type = region_type,
            .entry = entry,
            .exit = 0,
            .blocks = std.ArrayList(u32).empty,
            .children = std.ArrayList(*Region).empty,
            .allocator = allocator,
        };
        return region;
    }
    
    pub fn deinit(self: *Region) void {
        self.blocks.deinit(self.allocator);
        for (self.children.items) |child| {
            child.deinit();
            self.allocator.destroy(child);
        }
        self.children.deinit(self.allocator);
    }
};

/// Control flow structurer
pub const ControlFlowStructurer = struct {
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    dom_tree: *const DominatorTree,
    visited: std.AutoHashMap(u32, bool),
    
    pub fn init(allocator: std.mem.Allocator, cfg: *const CFG, dom_tree: *const DominatorTree) ControlFlowStructurer {
        return .{
            .allocator = allocator,
            .cfg = cfg,
            .dom_tree = dom_tree,
            .visited = std.AutoHashMap(u32, bool).init(allocator),
        };
    }
    
    pub fn deinit(self: *ControlFlowStructurer) void {
        self.visited.deinit();
    }
    
    /// Structure the entire function's control flow
    pub fn structure(self: *ControlFlowStructurer, entry: u32) !*Region {
        const root = Region.init(self.allocator, .sequence, entry);
        try self.structureRegion(root, entry);
        return root;
    }
    
    /// Recursively structure a region starting from an entry block
    fn structureRegion(self: *ControlFlowStructurer, region: *Region, entry: u32) !void {
        // Check if already visited
        if (self.visited.get(entry)) |v| {
            if (v) return;
        }
        try self.visited.put(entry, true);
        
        // Get the basic block
        const block = self.cfg.blocks.get(entry) orelse return;
        
        // Add block to region
        try region.blocks.append(self.allocator, entry);
        
        // Get outgoing edges
        const edges = self.cfg.edges.get(entry) orelse return;
        
        // Pattern match based on number and type of edges
        if (edges.items.len == 0) {
            // No successors - end of region
            region.exit = entry;
            return;
        } else if (edges.items.len == 1) {
            // Single successor - continue sequence
            const next = edges.items[0].to;
            try self.structureRegion(region, next);
        } else if (edges.items.len == 2) {
            // Two successors - potential if or loop
            try self.handleTwoWayBranch(region, entry, edges.items);
        } else {
            // Multiple successors - potential switch
            try self.handleMultiWayBranch(region, entry, edges.items);
        }
    }
    
    /// Handle a two-way branch (if-then-else or loop)
    fn handleTwoWayBranch(self: *ControlFlowStructurer, region: *Region, entry: u32, edges: []Edge) !void {
        _ = region;
        _ = entry;
        _ = edges;
        
        // Identify true and false branches
        var true_branch: ?u32 = null;
        var false_branch: ?u32 = null;
        
        for (edges) |edge| {
            switch (edge.type) {
                .conditional_true => true_branch = edge.to,
                .conditional_false => false_branch = edge.to,
                else => {},
            }
        }
        
        if (true_branch == null or false_branch == null) {
            // Not a proper conditional - treat as sequence
            return;
        }
        
        // Check if this is a loop (back edge)
        const is_loop = self.isBackEdge(entry, true_branch.?);
        
        if (is_loop) {
            try self.structureLoop(entry, true_branch.?, false_branch.?);
        } else {
            try self.structureConditional(entry, true_branch.?, false_branch.?);
        }
    }
    
    /// Handle a multi-way branch (switch/select case)
    fn handleMultiWayBranch(self: *ControlFlowStructurer, region: *Region, entry: u32, edges: []Edge) !void {
        _ = self;
        _ = region;
        _ = entry;
        _ = edges;
        
        // TODO: Implement switch/case detection
        // This would involve analyzing jump tables or multiple comparisons
    }
    
    /// Structure a loop
    fn structureLoop(self: *ControlFlowStructurer, header: u32, body: u32, exit: u32) !void {
        _ = self;
        _ = header;
        _ = body;
        _ = exit;
        
        // TODO: Implement loop structuring
        // Determine if it's a while, do-while, or for loop
        // Analyze loop condition and body
    }
    
    /// Structure a conditional (if-then-else)
    fn structureConditional(self: *ControlFlowStructurer, condition: u32, then_block: u32, else_block: u32) !void {
        _ = self;
        _ = condition;
        _ = then_block;
        _ = else_block;
        
        // TODO: Implement conditional structuring
        // Analyze both branches and find merge point
    }
    
    /// Check if an edge is a back edge (indicates a loop)
    fn isBackEdge(self: *ControlFlowStructurer, from: u32, to: u32) bool {
        // A back edge goes to a dominator
        return self.dom_tree.dominates(to, from);
    }
};

/// Loop pattern detector
pub const LoopDetector = struct {
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    dom_tree: *const DominatorTree,
    
    pub fn init(allocator: std.mem.Allocator, cfg: *const CFG, dom_tree: *const DominatorTree) LoopDetector {
        return .{
            .allocator = allocator,
            .cfg = cfg,
            .dom_tree = dom_tree,
        };
    }
    
    /// Detect natural loops in the CFG
    pub fn detectLoops(self: *LoopDetector) !std.ArrayList(Loop) {
        var loops = std.ArrayList(Loop).empty;
        
        // Find all back edges
        var it = self.cfg.edges.iterator();
        while (it.next()) |entry| {
            const from_block = entry.key_ptr.*;
            const edges = entry.value_ptr.*;
            
            for (edges.items) |edge| {
                const to_block = edge.to;
                
                // Check if this is a back edge
                if (self.dom_tree.dominates(to_block, from_block)) {
                    // Found a natural loop with header = to_block
                    const loop = try self.buildLoop(to_block, from_block);
                    try loops.append(self.allocator, loop);
                }
            }
        }
        
        return loops;
    }
    
    /// Build a loop structure from header and latch
    fn buildLoop(self: *LoopDetector, header: u32, latch: u32) !Loop {
        var loop = Loop{
            .header = header,
            .latch = latch,
            .body = std.ArrayList(u32).empty,
            .exits = std.ArrayList(u32).empty,
            .allocator = self.allocator,
        };
        
        // Find all blocks in the loop body using backward traversal
        var worklist = std.ArrayList(u32).empty;
        defer worklist.deinit(self.allocator);
        
        try worklist.append(self.allocator, latch);
        try loop.body.append(self.allocator, header);
        try loop.body.append(self.allocator, latch);
        
        while (worklist.items.len > 0) {
            const current = worklist.pop();
            
            // Get predecessors
            var edge_it = self.cfg.edges.iterator();
            while (edge_it.next()) |entry| {
                const from = entry.key_ptr.*;
                const edges = entry.value_ptr.*;
                
                for (edges.items) |edge| {
                    if (edge.to == current and from != header) {
                        // Check if already in body
                        var found = false;
                        for (loop.body.items) |b| {
                            if (b == from) {
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found) {
                            try loop.body.append(self.allocator, from);
                            try worklist.append(self.allocator, from);
                        }
                    }
                }
            }
        }
        
        return loop;
    }
};

pub const Loop = struct {
    header: u32,
    latch: u32,
    body: std.ArrayList(u32),
    exits: std.ArrayList(u32),
    allocator: std.mem.Allocator,
    
    pub fn deinit(self: *Loop) void {
        self.body.deinit(self.allocator);
        self.exits.deinit(self.allocator);
    }
};

/// Conditional pattern detector
pub const ConditionalDetector = struct {
    allocator: std.mem.Allocator,
    cfg: *const CFG,
    
    pub fn init(allocator: std.mem.Allocator, cfg: *const CFG) ConditionalDetector {
        return .{
            .allocator = allocator,
            .cfg = cfg,
        };
    }
    
    /// Detect if-then-else patterns
    pub fn detectConditional(self: *ConditionalDetector, block: u32) ?Conditional {
        const edges = self.cfg.edges.get(block) orelse return null;
        
        if (edges.items.len != 2) return null;
        
        var true_branch: ?u32 = null;
        var false_branch: ?u32 = null;
        
        for (edges.items) |edge| {
            switch (edge.type) {
                .conditional_true => true_branch = edge.to,
                .conditional_false => false_branch = edge.to,
                else => {},
            }
        }
        
        if (true_branch == null or false_branch == null) return null;
        
        // Find merge point
        const merge = self.findMergePoint(true_branch.?, false_branch.?) orelse return null;
        
        return Conditional{
            .condition_block = block,
            .then_block = true_branch.?,
            .else_block = false_branch.?,
            .merge_block = merge,
        };
    }
    
    /// Find the merge point of two branches
    fn findMergePoint(self: *ConditionalDetector, branch1: u32, branch2: u32) ?u32 {
        _ = self;
        // Simplified: assume branches merge at their immediate post-dominator
        // In a real implementation, we'd use post-dominator tree
        
        // For now, return null (not implemented)
        _ = branch1;
        _ = branch2;
        return null;
    }
};

pub const Conditional = struct {
    condition_block: u32,
    then_block: u32,
    else_block: u32,
    merge_block: u32,
};
