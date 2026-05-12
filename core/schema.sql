-- VBDecompiler Project Database Schema
-- SQLite database for storing analysis results and user annotations

-- Project metadata
CREATE TABLE IF NOT EXISTS project_info (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- Binary file information
CREATE TABLE IF NOT EXISTS binary_info (
    id INTEGER PRIMARY KEY,
    file_path TEXT NOT NULL,
    file_hash TEXT NOT NULL,
    image_base INTEGER NOT NULL,
    entry_point INTEGER NOT NULL,
    is_vb6 BOOLEAN NOT NULL,
    vb_version TEXT,
    compilation_type TEXT, -- Native or P-Code
    binary_type TEXT, -- EXE, DLL, OCX
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Functions/procedures
CREATE TABLE IF NOT EXISTS functions (
    id INTEGER PRIMARY KEY,
    address INTEGER NOT NULL UNIQUE,
    name TEXT,
    type TEXT, -- Sub, Function, PropertyGet, PropertyLet, PropertySet
    is_analyzed BOOLEAN DEFAULT 0,
    is_decompiled BOOLEAN DEFAULT 0,
    signature TEXT,
    return_type TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_functions_address ON functions(address);
CREATE INDEX IF NOT EXISTS idx_functions_name ON functions(name);

-- Function parameters
CREATE TABLE IF NOT EXISTS function_parameters (
    id INTEGER PRIMARY KEY,
    function_id INTEGER NOT NULL,
    param_index INTEGER NOT NULL,
    name TEXT,
    type TEXT,
    by_ref BOOLEAN DEFAULT 0,
    FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
);

-- Basic blocks
CREATE TABLE IF NOT EXISTS basic_blocks (
    id INTEGER PRIMARY KEY,
    address INTEGER NOT NULL,
    function_id INTEGER,
    size INTEGER NOT NULL,
    instruction_count INTEGER,
    FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_blocks_address ON basic_blocks(address);
CREATE INDEX IF NOT EXISTS idx_blocks_function ON basic_blocks(function_id);

-- Control flow edges
CREATE TABLE IF NOT EXISTS cfg_edges (
    id INTEGER PRIMARY KEY,
    from_block INTEGER NOT NULL,
    to_block INTEGER NOT NULL,
    edge_type TEXT, -- unconditional, conditional_true, conditional_false, call, return
    FOREIGN KEY (from_block) REFERENCES basic_blocks(id) ON DELETE CASCADE,
    FOREIGN KEY (to_block) REFERENCES basic_blocks(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_edges_from ON cfg_edges(from_block);
CREATE INDEX IF NOT EXISTS idx_edges_to ON cfg_edges(to_block);

-- Variables (SSA form)
CREATE TABLE IF NOT EXISTS variables (
    id INTEGER PRIMARY KEY,
    function_id INTEGER NOT NULL,
    var_id INTEGER NOT NULL, -- Hash from block_id + index
    name TEXT,
    inferred_type TEXT,
    FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_variables_function ON variables(function_id);
CREATE INDEX IF NOT EXISTS idx_variables_varid ON variables(var_id);

-- Cross-references
CREATE TABLE IF NOT EXISTS xrefs (
    id INTEGER PRIMARY KEY,
    from_address INTEGER NOT NULL,
    to_address INTEGER NOT NULL,
    xref_type TEXT, -- code, data, string, call, jump
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_xrefs_from ON xrefs(from_address);
CREATE INDEX IF NOT EXISTS idx_xrefs_to ON xrefs(to_address);

-- Strings
CREATE TABLE IF NOT EXISTS strings (
    id INTEGER PRIMARY KEY,
    address INTEGER NOT NULL,
    value TEXT NOT NULL,
    encoding TEXT, -- ascii, unicode
    length INTEGER NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_strings_address ON strings(address);

-- User comments
CREATE TABLE IF NOT EXISTS comments (
    id INTEGER PRIMARY KEY,
    address INTEGER NOT NULL,
    comment TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_comments_address ON comments(address);

-- User labels
CREATE TABLE IF NOT EXISTS labels (
    id INTEGER PRIMARY KEY,
    address INTEGER NOT NULL UNIQUE,
    label TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_labels_address ON labels(address);

-- Decompiled code cache
CREATE TABLE IF NOT EXISTS decompiled_code (
    id INTEGER PRIMARY KEY,
    function_id INTEGER NOT NULL UNIQUE,
    code TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
);

-- Analysis metadata (for incremental analysis)
CREATE TABLE IF NOT EXISTS analysis_state (
    id INTEGER PRIMARY KEY,
    address INTEGER NOT NULL UNIQUE,
    analysis_type TEXT NOT NULL, -- disasm, cfg, dataflow, ssa, types, decompile
    completed BOOLEAN DEFAULT 0,
    error TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_analysis_address ON analysis_state(address);
CREATE INDEX IF NOT EXISTS idx_analysis_type ON analysis_state(analysis_type);

-- Insert default project info
INSERT OR IGNORE INTO project_info (key, value) VALUES 
    ('schema_version', '1'),
    ('created_at', datetime('now')),
    ('vbdecompiler_version', '0.1.0');
