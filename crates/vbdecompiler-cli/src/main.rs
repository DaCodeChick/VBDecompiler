// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! VBDecompiler CLI - Command-line interface for decompiling VB5/6 executables

use clap::{CommandFactory, Parser, Subcommand};
use clap_complete::{generate, Shell};
use colored::Colorize;
use std::fs;
use std::io;
use std::path::PathBuf;
use vbdecompiler_core::{detect_packer, Decompiler, Error};

#[derive(Parser)]
#[command(name = "vbdc")]
#[command(author, version, about, long_about = None)]
#[command(propagate_version = true)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Enable verbose logging
    #[arg(short, long, global = true)]
    verbose: bool,

    /// Quiet mode (minimal output, errors only)
    #[arg(short, long, global = true)]
    quiet: bool,
}

#[derive(Subcommand)]
enum Commands {
    /// Decompile a VB executable
    Decompile {
        /// Path to VB executable (.exe, .dll, .ocx)
        #[arg(value_name = "FILE")]
        input: PathBuf,

        /// Output file or directory (default: stdout)
        #[arg(short, long, value_name = "PATH")]
        output: Option<PathBuf>,

        /// Output format
        #[arg(short, long, value_enum, default_value = "vb6")]
        format: OutputFormat,

        /// Force processing even if warnings detected
        #[arg(long)]
        force: bool,
    },

    /// Analyze a VB executable without decompiling
    Info {
        /// Path to VB executable
        #[arg(value_name = "FILE")]
        input: PathBuf,

        /// Show detailed information
        #[arg(short, long)]
        detailed: bool,

        /// Output format (text or json)
        #[arg(short, long, value_enum, default_value = "text")]
        format: InfoFormat,
    },

    /// Disassemble P-Code only (no decompilation)
    Disasm {
        /// Path to VB executable
        #[arg(value_name = "FILE")]
        input: PathBuf,

        /// Show hex bytes
        #[arg(short = 'x', long)]
        hex: bool,

        /// Output file (default: stdout)
        #[arg(short, long, value_name = "FILE")]
        output: Option<PathBuf>,
    },

    /// Check if executable is packed
    CheckPacker {
        /// Path to executable
        #[arg(value_name = "FILE")]
        input: PathBuf,
    },

    /// Generate shell completions
    Completions {
        /// Shell to generate completions for
        #[arg(value_enum)]
        shell: Shell,
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

#[derive(Clone, Copy, clap::ValueEnum)]
enum InfoFormat {
    /// Human-readable text
    Text,
    /// JSON output
    Json,
}

fn main() {
    let cli = Cli::parse();

    // Initialize logging
    let log_level = if cli.quiet {
        "error"
    } else if cli.verbose {
        "debug"
    } else {
        "info"
    };
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or(log_level)).init();

    // Execute command
    let result = match cli.command {
        Commands::Decompile {
            input,
            output,
            format,
            force,
        } => cmd_decompile(input, output, format, force, cli.quiet),
        Commands::Info {
            input,
            detailed,
            format,
        } => cmd_info(input, detailed, format, cli.quiet),
        Commands::Disasm { input, hex, output } => cmd_disasm(input, hex, output, cli.quiet),
        Commands::CheckPacker { input } => cmd_check_packer(input, cli.quiet),
        Commands::Completions { shell } => {
            cmd_completions(shell);
            return;
        }
    };

    // Handle errors
    if let Err(e) = result {
        eprintln!("{} {}", "Error:".red().bold(), e);
        std::process::exit(1);
    }
}

fn cmd_decompile(
    input: PathBuf,
    output: Option<PathBuf>,
    format: OutputFormat,
    _force: bool,
    quiet: bool,
) -> Result<(), Error> {
    if !quiet {
        println!("{} {}", "Decompiling:".green().bold(), input.display());
    }

    let mut decompiler = Decompiler::new();
    let result = decompiler.decompile_file(input.to_str().unwrap())?;

    // Generate output based on format
    let output_content = match format {
        OutputFormat::Vb6 => format_vb6(&result, quiet),
        OutputFormat::Json => format_json(&result)?,
        OutputFormat::Ir => format_ir(&result),
    };

    // Write to output
    if let Some(output_path) = output {
        // Determine if output is a directory or file
        if output_path.is_dir() {
            // Generate filename based on input
            let filename = input
                .file_stem()
                .unwrap_or_default()
                .to_string_lossy()
                .into_owned();
            let extension = match format {
                OutputFormat::Vb6 => "vb",
                OutputFormat::Json => "json",
                OutputFormat::Ir => "ir.txt",
            };
            let output_file = output_path.join(format!("{}.{}", filename, extension));

            fs::write(&output_file, output_content)?;

            if !quiet {
                println!(
                    "{} {}",
                    "Output written to:".green().bold(),
                    output_file.display()
                );
            }
        } else {
            // Write directly to file
            fs::write(&output_path, output_content)?;

            if !quiet {
                println!(
                    "{} {}",
                    "Output written to:".green().bold(),
                    output_path.display()
                );
            }
        }
    } else {
        // Write to stdout
        print!("{}", output_content);
    }

    Ok(())
}

fn format_vb6(result: &vbdecompiler_core::DecompilationResult, quiet: bool) -> String {
    let mut output = String::new();

    if !quiet {
        output.push_str(&format!("\n{}\n", "=".repeat(60)));
        output.push_str(&format!("Project: {}\n", result.project_name));
        output.push_str(&format!("P-Code: {}\n", result.is_pcode));
        output.push_str(&format!("Objects: {}\n", result.object_count));
        output.push_str(&format!("Methods: {}\n", result.method_count));
        output.push_str(&format!("{}\n\n", "=".repeat(60)));
    }

    output.push_str(&result.vb6_code);
    output
}

fn format_json(result: &vbdecompiler_core::DecompilationResult) -> Result<String, Error> {
    serde_json::to_string_pretty(result)
        .map_err(|e| Error::from(std::io::Error::new(std::io::ErrorKind::Other, e)))
}

fn format_ir(result: &vbdecompiler_core::DecompilationResult) -> String {
    // TODO: Implement IR formatting
    // For now, return a simple representation
    format!(
        "; IR Representation\n; Project: {}\n; Methods: {}\n\n{}",
        result.project_name, result.method_count, result.vb6_code
    )
}

fn cmd_info(input: PathBuf, detailed: bool, format: InfoFormat, quiet: bool) -> Result<(), Error> {
    if !quiet {
        println!("{} {}", "Analyzing:".green().bold(), input.display());
    }

    // Read and analyze file
    let data = fs::read(&input)?;

    // Basic PE info
    let pe_result = vbdecompiler_core::pe::PEFile::from_bytes(data.clone());

    // Packer detection
    let packer_result = detect_packer(&data);

    // Output based on format
    match format {
        InfoFormat::Text => {
            println!("\n{}", "=".repeat(60).blue());
            println!("{} {}", "File:".cyan().bold(), input.display());
            println!("{} {} bytes", "Size:".cyan().bold(), data.len());

            // Packer info
            match packer_result {
                Ok(Some(detection)) => {
                    println!(
                        "{} {} ({:.0}% confidence, via {:?})",
                        "Packer:".yellow().bold(),
                        detection.packer.name(),
                        detection.confidence * 100.0,
                        detection.method
                    );
                    println!("\n{}", "Unpacking instructions:".yellow());
                    println!("{}", detection.packer.unpack_instructions());
                }
                Ok(None) => {
                    println!("{} {}", "Packer:".cyan().bold(), "None detected");
                }
                Err(e) => {
                    println!("{} {}", "Packer detection error:".yellow(), e);
                }
            }

            // PE info
            match pe_result {
                Ok(pe) => {
                    println!("{} 0x{:08X}", "Image Base:".cyan().bold(), pe.image_base());
                    println!(
                        "{} 0x{:08X}",
                        "Entry Point:".cyan().bold(),
                        pe.entry_point()
                    );
                    println!("{} {}", "Is DLL:".cyan().bold(), pe.is_dll());
                    println!("{} {}", "Sections:".cyan().bold(), pe.sections().len());

                    if detailed {
                        println!("\n{}", "Section Table:".cyan().bold());
                        for section in pe.sections() {
                            let name = String::from_utf8_lossy(&section.name);
                            println!(
                                "  {} VA=0x{:08X} Size=0x{:08X}",
                                name.trim_end_matches('\0'),
                                section.virtual_address,
                                section.virtual_size
                            );
                        }

                        println!("\n{}", "Imported DLLs:".cyan().bold());
                        for dll in pe.imported_dlls() {
                            println!("  {}", dll);
                        }
                    }
                }
                Err(e) => {
                    println!("{} {}", "PE parsing error:".red(), e);
                }
            }

            println!("{}", "=".repeat(60).blue());
        }
        InfoFormat::Json => {
            // JSON output
            let json_data = serde_json::json!({
                "file": input.to_str(),
                "size": data.len(),
                "packer": packer_result.ok().and_then(|p| p.map(|d| serde_json::json!({
                    "name": d.packer.name(),
                    "confidence": d.confidence,
                    "method": format!("{:?}", d.method),
                }))),
                "pe": pe_result.as_ref().ok().map(|pe| serde_json::json!({
                    "image_base": format!("0x{:08X}", pe.image_base()),
                    "entry_point": format!("0x{:08X}", pe.entry_point()),
                    "is_dll": pe.is_dll(),
                    "section_count": pe.sections().len(),
                })),
            });
            println!("{}", serde_json::to_string_pretty(&json_data).unwrap());
        }
    }

    Ok(())
}

fn cmd_disasm(
    input: PathBuf,
    hex: bool,
    output: Option<PathBuf>,
    quiet: bool,
) -> Result<(), Error> {
    if !quiet {
        println!("{} {}", "Disassembling:".green().bold(), input.display());
    }

    // TODO: Full P-Code disassembly implementation
    // For now, provide basic disassembly info

    let mut decompiler = Decompiler::new();
    let result = decompiler.decompile_file(input.to_str().unwrap())?;

    let mut disasm_output = String::new();
    disasm_output.push_str(&format!("; P-Code Disassembly\n"));
    disasm_output.push_str(&format!("; Project: {}\n", result.project_name));
    disasm_output.push_str(&format!("; Methods: {}\n\n", result.method_count));

    if hex {
        disasm_output.push_str("; Hex dump mode enabled\n\n");
    }

    disasm_output.push_str("; TODO: Full P-Code disassembly not yet implemented\n");
    disasm_output.push_str("; Current output shows decompiled code:\n\n");
    disasm_output.push_str(&result.vb6_code);

    // Write output
    if let Some(output_path) = output {
        fs::write(&output_path, disasm_output)?;
        if !quiet {
            println!(
                "{} {}",
                "Disassembly written to:".green().bold(),
                output_path.display()
            );
        }
    } else {
        print!("{}", disasm_output);
    }

    Ok(())
}

fn cmd_check_packer(input: PathBuf, quiet: bool) -> Result<(), Error> {
    if !quiet {
        println!("{} {}", "Checking:".green().bold(), input.display());
    }

    let data = fs::read(&input)?;

    match detect_packer(&data) {
        Ok(Some(detection)) => {
            if quiet {
                println!("{}", detection.packer.name());
            } else {
                println!("\n{}", "✓ PACKER DETECTED".yellow().bold());
                println!("  {}: {}", "Packer".cyan(), detection.packer.name());
                println!(
                    "  {}: {:.1}%",
                    "Confidence".cyan(),
                    detection.confidence * 100.0
                );
                println!("  {}: {:?}", "Method".cyan(), detection.method);
                println!("\n{}", "Unpacking instructions:".cyan());
                println!("{}", detection.packer.unpack_instructions());
            }
            std::process::exit(1); // Exit code 1 = packed
        }
        Ok(None) => {
            if !quiet {
                println!("\n{}", "✓ No packer detected".green());
            }
            std::process::exit(0); // Exit code 0 = not packed
        }
        Err(e) => {
            return Err(Error::from(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("Packer detection failed: {}", e),
            )));
        }
    }
}

fn cmd_completions(shell: Shell) {
    let mut cmd = Cli::command();
    generate(shell, &mut cmd, "vbdc", &mut io::stdout());
}
