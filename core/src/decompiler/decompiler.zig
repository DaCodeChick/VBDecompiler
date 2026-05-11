// Main Decompiler - Orchestrates the decompilation process

const std = @import("std");
const SSAFunction = @import("../ir/ssa.zig").SSAFunction;
const SSABlock = @import("../ir/ssa.zig").SSABlock;
const CFG = @import("../analysis/cfg.zig").CFG;
const DominatorTree = @import("../analysis/dominators.zig").DominatorTree;
const TypeInferenceEngine = @import("../analysis/type_inference.zig").TypeInferenceEngine;
const vb6_ast = @import("vb6_ast.zig");
const ExpressionReconstructor = @import("expression_reconstruction.zig").ExpressionReconstructor;
const StatementReconstructor = @import("expression_reconstruction.zig").StatementReconstructor;
const VariableNamer = @import("variable_naming.zig").VariableNamer;
const VB6Printer = @import("vb6_printer.zig").VB6Printer;
const VB6PrinterOptions = @import("vb6_printer.zig").VB6PrinterOptions;

pub const DecompilerOptions = struct {
    generate_comments: bool = true,
    use_color: bool = false,
    indent_size: usize = 4,
};

/// Main decompiler
pub const Decompiler = struct {
    allocator: std.mem.Allocator,
    options: DecompilerOptions,
    
    pub fn init(allocator: std.mem.Allocator, options: DecompilerOptions) Decompiler {
        return .{
            .allocator = allocator,
            .options = options,
        };
    }
    
    /// Decompile an SSA function to VB6 code
    pub fn decompile(self: *Decompiler, 
                     ssa_func: *const SSAFunction,
                     cfg: *const CFG,
                     dom_tree: *const DominatorTree,
                     type_engine: *const TypeInferenceEngine,
                     address: u32) !vb6_ast.Procedure {
        
        // Create procedure name from address
        const proc_name = try std.fmt.allocPrint(self.allocator, "sub_{x:0>8}", .{address});
        
        var proc = vb6_ast.Procedure.init(
            self.allocator,
            proc_name,
            .sub,
            address
        );
        
        // Initialize variable namer
        var var_namer = VariableNamer.init(self.allocator, type_engine);
        defer var_namer.deinit();
        
        // Initialize expression reconstructor
        var expr_reconstructor = ExpressionReconstructor.init(
            self.allocator,
            ssa_func,
            type_engine
        );
        defer expr_reconstructor.deinit();
        
        // Initialize statement reconstructor
        var stmt_reconstructor = StatementReconstructor.init(
            self.allocator,
            &expr_reconstructor
        );
        
        // Generate variable declarations
        try self.generateVariableDeclarations(&proc, ssa_func, type_engine, &var_namer);
        
        // Reconstruct statements from SSA blocks
        try self.reconstructStatements(&proc, ssa_func, &stmt_reconstructor);
        
        _ = cfg;
        _ = dom_tree;
        
        return proc;
    }
    
    /// Generate variable declarations
    fn generateVariableDeclarations(
        self: *Decompiler,
        proc: *vb6_ast.Procedure,
        ssa_func: *const SSAFunction,
        type_engine: *const TypeInferenceEngine,
        var_namer: *VariableNamer
    ) !void {
        _ = ssa_func;
        
        // Iterate over all inferred types
        var it = type_engine.var_types.iterator();
        while (it.next()) |entry| {
            const var_id = entry.key_ptr.*;
            const vb_type = entry.value_ptr.*;
            
            // Skip bottom/top types
            if (vb_type == .bottom or vb_type == .top) continue;
            
            // Generate name
            const name = try var_namer.generateName(var_id, vb_type);
            
            // Add to local variables
            try proc.local_variables.append(self.allocator, vb6_ast.Variable{
                .name = name,
                .vb_type = vb_type,
            });
        }
    }
    
    /// Reconstruct statements from SSA
    fn reconstructStatements(
        self: *Decompiler,
        proc: *vb6_ast.Procedure,
        ssa_func: *const SSAFunction,
        stmt_reconstructor: *StatementReconstructor
    ) !void {
        _ = self;
        
        // Process blocks in order
        var block_addrs = std.ArrayList(u32).empty;
        defer block_addrs.deinit(proc.allocator);
        
        var it = ssa_func.blocks.keyIterator();
        while (it.next()) |addr| {
            try block_addrs.append(proc.allocator, addr.*);
        }
        
        // Sort by address
        std.mem.sort(u32, block_addrs.items, {}, comptime std.sort.asc(u32));
        
        // Process each block
        for (block_addrs.items) |block_addr| {
            if (ssa_func.blocks.get(block_addr)) |block| {
                // Add block comment
                const comment = try std.fmt.allocPrint(
                    proc.allocator,
                    "' Block 0x{x:0>8}",
                    .{block.address}
                );
                try proc.body.append(proc.allocator, vb6_ast.Statement{
                    .comment = comment,
                });
                
                // Process each instruction
                for (block.instructions.items) |*inst| {
                    const stmt = try stmt_reconstructor.reconstructStatement(inst);
                    try proc.body.append(proc.allocator, stmt);
                }
            }
        }
    }
    
    /// Print decompiled code to writer
    pub fn printToWriter(
        self: *Decompiler,
        writer: anytype,
        proc: *const vb6_ast.Procedure
    ) !void {
        var printer = VB6Printer.init(self.allocator, VB6PrinterOptions{
            .indent_size = self.options.indent_size,
            .use_color = self.options.use_color,
        });
        
        try printer.printProcedure(writer, proc);
    }
};
