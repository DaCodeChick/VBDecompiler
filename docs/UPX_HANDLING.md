# UPX Compression Handling

## Issue

Many VB5/6 executables in the wild are compressed with UPX (Ultimate Packer for eXecutables). When compressed:
- VB5! signature is still visible in the compressed data
- VB header structure can be partially read
- **However**, internal pointers (`lpProjectInfo`, etc.) are encrypted/scrambled
- These pointers are fixed up at runtime during decompression

## Example: Phalanx.exe

### Compressed (681 KB):
```
lpProjectInfo = 0x592B10FD  (invalid - way beyond file size)
imageBase     = 0x400000
RVA           = 0x58EB10FD  (1.4 GB - clearly invalid!)
```

### Uncompressed (2.9 MB):
```
lpProjectInfo = 0x449910
imageBase     = 0x400000
RVA           = 0x49910     (valid - within file bounds)
```

## Solutions

### 1. Manual Unpacking (Current Approach)
```bash
# Create decompressed copy for analysis
upx -d target.exe -o target_unpacked.exe
```

**Pros:**
- Simple and reliable
- Works with all UPX versions
- No code changes needed

**Cons:**
- Requires UPX installed
- Manual step before analysis

### 2. Automatic Detection & Unpacking (Future)
Detect UPX sections and:
- Option A: Shell out to `upx -d` automatically
- Option B: Implement UPX decompression in C++ (complex)
- Option C: Warn user to unpack manually

### 3. Runtime Memory Analysis (Advanced)
- Run executable in sandboxed environment
- Read VB structures from process memory after decompression
- Complex but handles all packers

## Recommendation

For MVP: **Document limitation** and provide clear error message when UPX is detected.

For v1.0: **Add automatic UPX detection** and prompt user to unpack or do it automatically.

## Detection

Check for UPX sections:
```cpp
bool isUPXPacked(const PEFile& pe) {
    for (const auto& section : pe.getSections()) {
        std::string name = section.getName();
        if (name.find("UPX") == 0) {  // UPX0, UPX1, UPX2
            return true;
        }
    }
    return false;
}
```

## User-Facing Error Message

When UPX detected:
```
This executable is compressed with UPX. VB structures cannot be read from
compressed executables.

To analyze this file, please decompress it first:
  upx -d target.exe -o target_unpacked.exe

Then open the unpacked file in VBDecompiler.
```
