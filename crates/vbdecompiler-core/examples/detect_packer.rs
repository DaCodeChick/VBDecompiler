//! Test packer detection on real executables

use std::env;
use std::fs;
use vbdecompiler_core::packer::detect_packer;

fn main() {
    let args: Vec<String> = env::args().collect();
    let path = if args.len() > 1 {
        &args[1]
    } else {
        "/home/admin/.wine/drive_c/Program Files (x86)/Brainhouse Labs/Phalanx/Phalanx.exe"
    };

    println!("Testing packer detection on: {}", path);
    println!("Reading file...");

    let data = match fs::read(path) {
        Ok(d) => d,
        Err(e) => {
            eprintln!("Failed to read file: {}", e);
            return;
        }
    };

    println!("File size: {} bytes", data.len());
    println!("Running packer detection...\n");

    match detect_packer(&data) {
        Ok(Some(detection)) => {
            println!("✓ PACKER DETECTED!");
            println!("  Packer: {}", detection.packer.name());
            println!("  Confidence: {:.1}%", detection.confidence * 100.0);
            println!("  Detection method: {:?}", detection.method);
            println!("\nUnpacking instructions:");
            println!("{}", detection.packer.unpack_instructions());
        }
        Ok(None) => {
            println!("✗ No packer detected - file appears unpacked");
        }
        Err(e) => {
            eprintln!("✗ Error during detection: {}", e);
        }
    }
}
