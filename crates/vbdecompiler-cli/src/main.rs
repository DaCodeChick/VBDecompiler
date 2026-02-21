// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! VBDecompiler CLI - Command-line interface for decompiling VB5/6 executables

use clap::{Parser, Subcommand};
use colored::Colorize;
use std::path::PathBuf;
use vbdecompiler_core::{Decompiler, Error};

#[derive(Parser)]
#[command(name = "vbdecompiler")]
#[command(author, version, about, long_about = None)]
#[command(propagate_version = true)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Enable verbose logging
    #[arg(short, long, global = true)]
    verbose: bool,
}

#[derive(Subcommand)]
enum Commands {
    /// Decompile a VB executable
    Decompile {
        /// Path to VB executable (.exe, .dll, .ocx)
        #[arg(value_name = "FILE")]
        input: PathBuf,

        /// Output directory (default: stdout)
        #[arg(short, long, value_name = "DIR")]
        output: Option<PathBuf>,

        /// Output format
        #[arg(short, long, value_enum, default_value = "vb6")]
        format: OutputFormat,
    },

    /// Analyze a VB executable without decompiling
    Info {
        /// Path to VB executable
        #[arg(value_name = "FILE")]
        input: PathBuf,

        /// Show detailed information
        #[arg(short, long)]
        detailed: bool,
    },

    /// Disassemble P-Code only (no decompilation)
    Disasm {
        /// Path to VB executable
        #[arg(value_name = "FILE")]
        input: PathBuf,

        /// Show hex bytes
        #[arg(short = 'x', long)]
        hex: bool,
    },
}

#[derive(Clone, Copy, clap::ValueEnum)]
enum OutputFormat {
    /// VB6 source code
    Vb6,
    /// JSON representation
    Json,
    /// IR (Intermediate Representation)
    Ir,
}

fn main() {
    let cli = Cli::parse();

    // Initialize logging
    let log_level = if cli.verbose { "debug" } else { "info" };
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or(log_level)).init();

    // Execute command
    let result = match cli.command {
        Commands::Decompile {
            input,
            output,
            format,
        } => cmd_decompile(input, output, format),
        Commands::Info { input, detailed } => cmd_info(input, detailed),
        Commands::Disasm { input, hex } => cmd_disasm(input, hex),
    };

    // Handle errors
    if let Err(e) = result {
        eprintln!("{} {}", "Error:".red().bold(), e);
        std::process::exit(1);
    }
}

fn cmd_decompile(
    input: PathBuf,
    _output: Option<PathBuf>,
    _format: OutputFormat,
) -> Result<(), Error> {
    println!("{} {}", "Decompiling:".green().bold(), input.display());

    let mut decompiler = Decompiler::new();
    let result = decompiler.decompile_file(input.to_str().unwrap())?;

    println!("\n{}", "=".repeat(60).blue());
    println!("{} {}", "Project:".cyan().bold(), result.project_name);
    println!("{} {}", "P-Code:".cyan().bold(), result.is_pcode);
    println!("{} {}", "Objects:".cyan().bold(), result.object_count);
    println!("{} {}", "Methods:".cyan().bold(), result.method_count);
    println!("{}", "=".repeat(60).blue());
    println!();
    println!("{}", result.vb6_code);

    Ok(())
}

fn cmd_info(input: PathBuf, _detailed: bool) -> Result<(), Error> {
    println!("{} {}", "Analyzing:".green().bold(), input.display());

    // TODO: Implement info command
    println!("{}", "Not implemented yet".yellow());

    Ok(())
}

fn cmd_disasm(input: PathBuf, _hex: bool) -> Result<(), Error> {
    println!("{} {}", "Disassembling:".green().bold(), input.display());

    // TODO: Implement disasm command
    println!("{}", "Not implemented yet".yellow());

    Ok(())
}
