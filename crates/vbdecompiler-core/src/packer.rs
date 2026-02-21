//! Executable packer detection
//!
//! This module detects common executable packers/compressors used with VB executables.
//! Detection methods include:
//! - Section name analysis (UPX, ASPack, PECompact signatures)
//! - Entropy analysis (high entropy indicates compression/encryption)
//! - Import table characteristics
//!
//! Common packers for VB5/VB6 executables:
//! - UPX (Ultimate Packer for eXecutables) - Most common
//! - ASPack - Commercial packer
//! - PECompact - Commercial packer
//! - Themida/WinLicense - Advanced protection
//! - FSG (Fast Small Good) - Small free packer
//! - Petite - Fast packer

use goblin::pe::PE;
use thiserror::Error;

/// Error type for packer detection
#[derive(Debug, Error)]
pub enum PackerError {
    #[error("Failed to parse PE: {0}")]
    ParseError(String),

    #[error("Invalid PE data")]
    InvalidData,
}

/// Types of detected packers
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PackerType {
    /// UPX (Ultimate Packer for eXecutables)
    UPX,

    /// ASPack commercial packer
    ASPack,

    /// PECompact commercial packer
    PECompact,

    /// Themida/WinLicense protection
    Themida,

    /// FSG (Fast Small Good) packer
    FSG,

    /// Petite packer
    Petite,

    /// MEW (Modified EXE Wrapping)
    MEW,

    /// NSPack
    NSPack,

    /// Unknown packer detected via heuristics
    Unknown,
}

impl PackerType {
    /// Get human-readable name
    pub fn name(&self) -> &'static str {
        match self {
            PackerType::UPX => "UPX",
            PackerType::ASPack => "ASPack",
            PackerType::PECompact => "PECompact",
            PackerType::Themida => "Themida/WinLicense",
            PackerType::FSG => "FSG",
            PackerType::Petite => "Petite",
            PackerType::MEW => "MEW",
            PackerType::NSPack => "NSPack",
            PackerType::Unknown => "Unknown",
        }
    }

    /// Get instructions for unpacking
    pub fn unpack_instructions(&self) -> &'static str {
        match self {
            PackerType::UPX => {
                "Install UPX (https://upx.github.io/) and run:\n  upx -d <file>"
            }
            PackerType::ASPack => {
                "Use ASPack unpacker or a universal unpacker tool"
            }
            PackerType::PECompact => {
                "Use PECompact unpacker or a universal unpacker tool"
            }
            PackerType::Themida => {
                "Themida uses advanced protection. Manual unpacking or specialized tools required."
            }
            PackerType::FSG => {
                "Use FSG unpacker or a universal unpacker tool"
            }
            PackerType::Petite => {
                "Use Petite unpacker or a universal unpacker tool"
            }
            PackerType::MEW => {
                "Use MEW unpacker or a universal unpacker tool"
            }
            PackerType::NSPack => {
                "Use NSPack unpacker or a universal unpacker tool"
            }
            PackerType::Unknown => {
                "Manual unpacking required. Try universal unpackers like:\n  - UPX\n  - UniversalUnpacker\n  - PE-Bear"
            }
        }
    }
}

/// Packer detection result
#[derive(Debug)]
pub struct PackerDetection {
    /// Detected packer type
    pub packer: PackerType,

    /// Detection confidence (0.0 - 1.0)
    pub confidence: f64,

    /// Detection method used
    pub method: DetectionMethod,
}

/// Method used to detect packer
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DetectionMethod {
    /// Section name signature
    SectionName,

    /// High entropy analysis
    Entropy,

    /// Import table characteristics
    ImportTable,

    /// Multiple methods agree
    Combined,
}

/// High entropy threshold (0-8 scale, 8 = maximum entropy)
const HIGH_ENTROPY_THRESHOLD: f64 = 7.2;

/// Detect if a PE executable is packed
pub fn detect_packer(pe_data: &[u8]) -> Result<Option<PackerDetection>, PackerError> {
    // Try lightweight section name detection first (doesn't parse full PE)
    // This works even on packed files where resources are corrupted
    if let Some(detection) = detect_by_section_names_raw(pe_data) {
        return Ok(Some(detection));
    }

    // Now try full PE parse for more sophisticated detection
    // Use parse_unchecked to skip resource validation
    let pe = match PE::parse(pe_data) {
        Ok(pe) => pe,
        Err(_) => {
            // If full parse fails (e.g., corrupted resources in packed file),
            // fall back to basic entropy analysis on the whole file
            return Ok(detect_by_raw_entropy(pe_data));
        }
    };

    // Try section name detection with full PE
    if let Some(detection) = detect_by_section_names(&pe) {
        return Ok(Some(detection));
    }

    // Try entropy analysis (medium confidence)
    if let Some(detection) = detect_by_entropy(&pe, pe_data) {
        return Ok(Some(detection));
    }

    // Try import table analysis (low confidence)
    if let Some(detection) = detect_by_imports(&pe) {
        return Ok(Some(detection));
    }

    Ok(None)
}

/// Detect packer by section names
fn detect_by_section_names(pe: &PE) -> Option<PackerDetection> {
    for section in &pe.sections {
        let name = String::from_utf8_lossy(&section.name);
        let name_trimmed = name.trim_end_matches('\0');

        // UPX signatures
        if name_trimmed.starts_with("UPX") {
            return Some(PackerDetection {
                packer: PackerType::UPX,
                confidence: 0.95,
                method: DetectionMethod::SectionName,
            });
        }

        // ASPack signatures
        if name_trimmed.starts_with(".aspack") || name_trimmed.starts_with(".adata") {
            return Some(PackerDetection {
                packer: PackerType::ASPack,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // PECompact signatures
        if name_trimmed.starts_with("PEC2") || name_trimmed.starts_with("PECompact") {
            return Some(PackerDetection {
                packer: PackerType::PECompact,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // Themida/WinLicense signatures
        if name_trimmed.starts_with(".themida") || name_trimmed.starts_with(".winlice") {
            return Some(PackerDetection {
                packer: PackerType::Themida,
                confidence: 0.95,
                method: DetectionMethod::SectionName,
            });
        }

        // FSG signatures
        if name_trimmed.eq_ignore_ascii_case("FSG!") {
            return Some(PackerDetection {
                packer: PackerType::FSG,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // Petite signatures
        if name_trimmed.starts_with(".petite") {
            return Some(PackerDetection {
                packer: PackerType::Petite,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // MEW signatures
        if name_trimmed.eq_ignore_ascii_case("MEW") {
            return Some(PackerDetection {
                packer: PackerType::MEW,
                confidence: 0.85,
                method: DetectionMethod::SectionName,
            });
        }

        // NSPack signatures
        if name_trimmed.starts_with(".nsp") {
            return Some(PackerDetection {
                packer: PackerType::NSPack,
                confidence: 0.85,
                method: DetectionMethod::SectionName,
            });
        }
    }

    None
}

/// Detect packer by section names using raw PE parsing
/// This is more robust than full PE parsing for packed files
fn detect_by_section_names_raw(pe_data: &[u8]) -> Option<PackerDetection> {
    // Minimal PE header parsing to read sections
    // PE signature offset is at 0x3C
    if pe_data.len() < 0x40 {
        return None;
    }

    let pe_offset =
        u32::from_le_bytes([pe_data[0x3C], pe_data[0x3D], pe_data[0x3E], pe_data[0x3F]]) as usize;

    // Check we have room for PE signature + COFF header
    if pe_offset + 24 > pe_data.len() {
        return None;
    }

    // Verify PE signature "PE\0\0"
    if &pe_data[pe_offset..pe_offset + 4] != b"PE\0\0" {
        return None;
    }

    // Number of sections is at offset 6 in COFF header
    let num_sections = u16::from_le_bytes([pe_data[pe_offset + 6], pe_data[pe_offset + 7]]);

    // Size of optional header
    let opt_header_size =
        u16::from_le_bytes([pe_data[pe_offset + 20], pe_data[pe_offset + 21]]) as usize;

    // Section table starts after PE signature (4) + COFF header (20) + optional header
    let section_table_offset = pe_offset + 24 + opt_header_size;

    // Each section entry is 40 bytes, first 8 bytes are the name
    for i in 0..num_sections {
        let section_offset = section_table_offset + (i as usize * 40);

        if section_offset + 8 > pe_data.len() {
            break;
        }

        let section_name = &pe_data[section_offset..section_offset + 8];
        let name = String::from_utf8_lossy(section_name);
        let name_trimmed = name.trim_end_matches('\0');

        // UPX signatures
        if name_trimmed.starts_with("UPX") {
            return Some(PackerDetection {
                packer: PackerType::UPX,
                confidence: 0.95,
                method: DetectionMethod::SectionName,
            });
        }

        // ASPack signatures
        if name_trimmed.starts_with(".aspack") || name_trimmed.starts_with(".adata") {
            return Some(PackerDetection {
                packer: PackerType::ASPack,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // PECompact signatures
        if name_trimmed.starts_with("PEC2") || name_trimmed.starts_with("PECompact") {
            return Some(PackerDetection {
                packer: PackerType::PECompact,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // Themida/WinLicense signatures
        if name_trimmed.starts_with(".themida") || name_trimmed.starts_with(".winlice") {
            return Some(PackerDetection {
                packer: PackerType::Themida,
                confidence: 0.95,
                method: DetectionMethod::SectionName,
            });
        }

        // FSG signatures
        if name_trimmed.eq_ignore_ascii_case("FSG!") {
            return Some(PackerDetection {
                packer: PackerType::FSG,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // Petite signatures
        if name_trimmed.starts_with(".petite") {
            return Some(PackerDetection {
                packer: PackerType::Petite,
                confidence: 0.90,
                method: DetectionMethod::SectionName,
            });
        }

        // MEW signatures
        if name_trimmed.eq_ignore_ascii_case("MEW") {
            return Some(PackerDetection {
                packer: PackerType::MEW,
                confidence: 0.85,
                method: DetectionMethod::SectionName,
            });
        }

        // NSPack signatures
        if name_trimmed.starts_with(".nsp") {
            return Some(PackerDetection {
                packer: PackerType::NSPack,
                confidence: 0.85,
                method: DetectionMethod::SectionName,
            });
        }
    }

    None
}

/// Detect packer by whole-file entropy (fallback when PE parse fails)
fn detect_by_raw_entropy(pe_data: &[u8]) -> Option<PackerDetection> {
    // Sample first 64KB for performance
    let sample_size = std::cmp::min(65536, pe_data.len());
    let entropy = calculate_shannon_entropy(&pe_data[..sample_size]);

    if entropy > HIGH_ENTROPY_THRESHOLD {
        return Some(PackerDetection {
            packer: PackerType::Unknown,
            confidence: 0.60,
            method: DetectionMethod::Entropy,
        });
    }

    None
}

/// Detect packer by entropy analysis
fn detect_by_entropy(pe: &PE, pe_data: &[u8]) -> Option<PackerDetection> {
    let mut high_entropy_count = 0;
    let mut total_sections = 0;

    for section in &pe.sections {
        if section.size_of_raw_data == 0 {
            continue;
        }

        let start = section.pointer_to_raw_data as usize;
        let size = section.size_of_raw_data as usize;

        // Bounds check
        if start >= pe_data.len() || start + size > pe_data.len() {
            continue;
        }

        let section_data = &pe_data[start..start + size];
        let section_entropy = calculate_shannon_entropy(section_data);

        total_sections += 1;

        if section_entropy > HIGH_ENTROPY_THRESHOLD {
            high_entropy_count += 1;
        }
    }

    // If most sections have high entropy, likely packed
    if total_sections > 0 && high_entropy_count as f64 / total_sections as f64 > 0.6 {
        return Some(PackerDetection {
            packer: PackerType::Unknown,
            confidence: 0.70,
            method: DetectionMethod::Entropy,
        });
    }

    None
}

/// Detect packer by import table characteristics
fn detect_by_imports(pe: &PE) -> Option<PackerDetection> {
    // Packers often have very few imports
    let import_count = pe.imports.len();

    // Normal VB executables have many imports
    // Very few imports (< 5) suggests packing
    if import_count < 5 {
        return Some(PackerDetection {
            packer: PackerType::Unknown,
            confidence: 0.50,
            method: DetectionMethod::ImportTable,
        });
    }

    None
}

/// Calculate Shannon entropy for a byte slice
/// Returns value from 0.0 (no entropy) to 8.0 (maximum entropy)
fn calculate_shannon_entropy(data: &[u8]) -> f64 {
    if data.is_empty() {
        return 0.0;
    }

    // Count byte frequencies
    let mut freq = [0u32; 256];
    for &byte in data {
        freq[byte as usize] += 1;
    }

    let len = data.len() as f64;
    let mut entropy = 0.0;

    for &count in &freq {
        if count > 0 {
            let p = count as f64 / len;
            entropy -= p * p.log2();
        }
    }

    entropy
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shannon_entropy_uniform() {
        // Uniform distribution should have high entropy
        let data: Vec<u8> = (0..=255).collect();
        let entropy = calculate_shannon_entropy(&data);
        assert!(
            entropy > 7.5,
            "Uniform distribution should have entropy > 7.5, got {}",
            entropy
        );
    }

    #[test]
    fn test_shannon_entropy_low() {
        // Repetitive data should have low entropy
        let data = vec![0u8; 1000];
        let entropy = calculate_shannon_entropy(&data);
        assert!(
            entropy < 0.1,
            "Repetitive data should have entropy < 0.1, got {}",
            entropy
        );
    }

    #[test]
    fn test_shannon_entropy_empty() {
        let entropy = calculate_shannon_entropy(&[]);
        assert_eq!(entropy, 0.0);
    }

    #[test]
    fn test_packer_type_name() {
        assert_eq!(PackerType::UPX.name(), "UPX");
        assert_eq!(PackerType::ASPack.name(), "ASPack");
        assert_eq!(PackerType::Unknown.name(), "Unknown");
    }

    #[test]
    fn test_packer_unpack_instructions() {
        let instr = PackerType::UPX.unpack_instructions();
        assert!(instr.contains("upx"));
        assert!(instr.contains("-d"));
    }
}
