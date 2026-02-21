// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! PE file parsing module
//!
//! Parses Windows PE (Portable Executable) files to extract:
//! - PE headers (DOS, NT, Optional)
//! - Section headers and data
//! - Import tables
//! - Resource sections
//! - Packer detection

use crate::error::{Error, Result};
use crate::packer::detect_packer;
use goblin::pe::{section_table::SectionTable, PE};
use std::path::Path;

/// Maximum size for a single read operation (100MB)
const MAX_READ_SIZE: usize = 100 * 1024 * 1024;

/// PE file parser
pub struct PEFile {
    /// Raw file data
    data: Vec<u8>,
    /// Parsed PE structure from goblin
    pe: PE<'static>,
    /// Image base address
    image_base: u32,
    /// Entry point RVA
    entry_point: u32,
}

impl PEFile {
    /// Parse a PE file from a path
    pub fn from_path(path: impl AsRef<Path>) -> Result<Self> {
        let data = std::fs::read(path.as_ref())?;
        Self::from_bytes(data)
    }

    /// Parse a PE file from bytes
    pub fn from_bytes(mut data: Vec<u8>) -> Result<Self> {
        if data.len() < 64 {
            return Err(Error::invalid_pe("File too small to contain DOS header"));
        }

        // DOS signature check
        if &data[0..2] != b"MZ" {
            return Err(Error::invalid_pe("Invalid DOS signature"));
        }

        // Check for packers early
        if let Ok(Some(detection)) = detect_packer(&data) {
            log::warn!(
                "Packed executable detected: {} (confidence: {:.0}%)",
                detection.packer.name(),
                detection.confidence * 100.0
            );
            log::warn!("Unpacking instructions:");
            log::warn!("{}", detection.packer.unpack_instructions());

            return Err(Error::invalid_pe(format!(
                "Packed executable detected ({}). Please unpack before decompilation.\n{}",
                detection.packer.name(),
                detection.packer.unpack_instructions()
            )));
        }

        // VB6 executables often have non-standard resource structures that goblin can't parse,
        // but resources aren't needed for VB decompilation (we only need headers, sections, imports).
        // Proactively remove the resource directory to avoid parsing issues.
        if let Some(fixed_data) = Self::try_remove_resource_directory(&data) {
            log::debug!("Removed resource directory to avoid VB6 compatibility issues");
            data = fixed_data;
        }

        // Try parsing with permissive mode
        let mut opts = goblin::pe::options::ParseOptions::default();
        opts.parse_mode = goblin::options::ParseMode::Permissive;

        // Parse PE using goblin
        // SAFETY: We need to transmute the lifetime to 'static to store the PE struct.
        // The PE struct holds references into the data vector, and we ensure both live
        // for the same lifetime by storing them together in PEFile.
        let pe: PE<'static> = unsafe {
            let data_ptr = data.as_ptr();
            let data_len = data.len();
            let static_slice = std::slice::from_raw_parts(data_ptr, data_len);
            goblin::pe::PE::parse_with_opts(static_slice, &opts)
                .map_err(|e| Error::invalid_pe(format!("Failed to parse PE file: {}", e)))?
        };

        // Continue with rest of validation
        Self::validate_and_create(data, pe)
    }

    /// Try to remove the resource directory entry from PE optional header
    fn try_remove_resource_directory(data: &[u8]) -> Option<Vec<u8>> {
        if data.len() < 0x3c + 4 {
            return None;
        }

        // Read PE offset from DOS header
        let pe_offset =
            u32::from_le_bytes([data[0x3c], data[0x3c + 1], data[0x3c + 2], data[0x3c + 3]])
                as usize;

        // Optional header starts after PE signature (4 bytes) + COFF header (20 bytes)
        let opt_header_offset = pe_offset + 4 + 20;
        // Resource directory entry is at offset 112 in optional header (for PE32)
        let resource_dir_offset = opt_header_offset + 112;

        if data.len() < resource_dir_offset + 8 {
            return None;
        }

        // Create a copy and zero out resource directory entry (8 bytes: RVA + Size)
        let mut data_copy = data.to_vec();
        for i in resource_dir_offset..resource_dir_offset + 8 {
            data_copy[i] = 0;
        }

        Some(data_copy)
    }

    /// Validate PE and create PEFile struct (extracted to reduce duplication)
    fn validate_and_create(data: Vec<u8>, pe: PE<'static>) -> Result<Self> {
        // Validate PE type
        if !pe.is_lib && pe.header.optional_header.is_none() {
            return Err(Error::invalid_pe("Invalid PE optional header"));
        }

        // Extract image base and entry point
        let (image_base, entry_point) = if let Some(opt_header) = &pe.header.optional_header {
            let base = opt_header.windows_fields.image_base as u32;
            let entry = opt_header.standard_fields.address_of_entry_point as u32;
            (base, entry)
        } else {
            (0x400000, 0) // Default values
        };

        // Verify we're dealing with a 32-bit PE
        if let Some(opt_header) = &pe.header.optional_header {
            if opt_header.standard_fields.magic != goblin::pe::optional_header::MAGIC_32 {
                return Err(Error::invalid_pe("Only 32-bit PE files are supported"));
            }
        }

        // Verify it's an x86 executable
        if pe.header.coff_header.machine != goblin::pe::header::COFF_MACHINE_X86 {
            return Err(Error::invalid_pe("Only x86 executables are supported"));
        }

        Ok(Self {
            data,
            pe,
            image_base,
            entry_point,
        })
    }

    /// Get the image base address
    pub fn image_base(&self) -> u32 {
        self.image_base
    }

    /// Get the entry point RVA
    pub fn entry_point(&self) -> u32 {
        self.entry_point
    }

    /// Get raw file data
    pub fn data(&self) -> &[u8] {
        &self.data
    }

    /// Check if this is a DLL
    pub fn is_dll(&self) -> bool {
        self.pe.is_lib
    }

    /// Check if this is an executable
    pub fn is_executable(&self) -> bool {
        (self.pe.header.coff_header.characteristics & 0x0002) != 0
    }

    /// Get all section headers
    pub fn sections(&self) -> &[SectionTable] {
        &self.pe.sections
    }

    /// Get a section by name
    pub fn section_by_name(&self, name: &str) -> Option<&SectionTable> {
        self.pe
            .sections
            .iter()
            .find(|s| s.name().map(|n| n == name).unwrap_or(false))
    }

    /// Get a section containing the given RVA
    pub fn section_by_rva(&self, rva: u32) -> Option<&SectionTable> {
        self.pe.sections.iter().find(|s| {
            let start = s.virtual_address;
            let end = start + s.virtual_size;
            rva >= start && rva < end
        })
    }

    /// Convert RVA to file offset
    pub fn rva_to_offset(&self, rva: u32) -> Option<usize> {
        let section = self.section_by_rva(rva)?;

        // Calculate offset within section
        let section_offset = rva.checked_sub(section.virtual_address)?;

        // Convert to file offset
        let file_offset = section.pointer_to_raw_data.checked_add(section_offset)?;

        Some(file_offset as usize)
    }

    /// Read data at a given RVA
    ///
    /// Returns None if the RVA is invalid or if the requested size exceeds MAX_READ_SIZE.
    pub fn read_at_rva(&self, rva: u32, size: usize) -> Option<&[u8]> {
        // Sanity check: refuse to read more than 100MB
        if size > MAX_READ_SIZE {
            return None;
        }

        // Convert RVA to file offset
        let offset = self.rva_to_offset(rva)?;

        // Check bounds
        if offset >= self.data.len() {
            return None;
        }

        // Clamp size to available data
        let available = self.data.len() - offset;
        let size = size.min(available);

        if size == 0 {
            return None;
        }

        Some(&self.data[offset..offset + size])
    }

    /// Read data at a given RVA into a vector
    ///
    /// Returns an empty vector if the RVA is invalid or if the requested size exceeds MAX_READ_SIZE.
    pub fn read_at_rva_vec(&self, rva: u32, size: usize) -> Vec<u8> {
        self.read_at_rva(rva, size)
            .map(|slice| slice.to_vec())
            .unwrap_or_default()
    }

    /// Get list of imported DLL names
    pub fn imported_dlls(&self) -> Vec<String> {
        let mut dlls = Vec::new();
        let mut seen = std::collections::HashSet::new();

        for import in &self.pe.imports {
            let dll = import.dll.to_string();
            if seen.insert(dll.clone()) {
                dlls.push(dll);
            }
        }

        dlls
    }

    /// Get imported functions from a specific DLL
    pub fn imports_from_dll(&self, dll_name: &str) -> Vec<String> {
        self.pe
            .imports
            .iter()
            .filter(|import| import.dll.eq_ignore_ascii_case(dll_name))
            .map(|import| import.name.to_string())
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_invalid_dos_signature() {
        let data = vec![0x00, 0x00]; // Invalid signature
        let result = PEFile::from_bytes(data);
        assert!(result.is_err());
    }

    #[test]
    fn test_file_too_small() {
        let data = vec![0x4D, 0x5A]; // "MZ" but too small
        let result = PEFile::from_bytes(data);
        assert!(result.is_err());
    }
}
