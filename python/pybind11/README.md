# UHDM Python Bindings (pybind11)

This directory contains **pybind11-based Python bindings** for the UHDM (Universal Hardware Data Model) library.

## Milestone 1: Load/Save Functionality

The current implementation provides a `UHDMDatabase` class that can:
- **Load** UHDM binary files from disk
- **Save** UHDM data back to binary files

## Building the Module

### Prerequisites

- CMake 3.20+
- Python 3.8+ with development headers
- C++17 compatible compiler
- UHDM library (built from the parent project)

### Build Commands

From the UHDM root directory:

```bash
# Configure the build (includes pybind11 module)
cmake -S . -B build_pybind11

# Build the pyuhdm module
cmake --build build_pybind11 --target pyuhdm

# The module will be located at:
# build_pybind11/lib/pyuhdm.cpython-<version>-<platform>.so
```

### Verifying the Build

After building, you can verify the module loads correctly:

```bash
# From the UHDM root directory
python -c "import sys; sys.path.insert(0, 'build_pybind11/lib'); import pyuhdm; print('pyuhdm loaded successfully')"
```

## Running the Verification Script

A verification script is provided to test the load/save functionality:

```bash
# Run with a .uhdm file
python python/pybind11/verify_milestone1.py <path_to_file.uhdm>

# Or specify an output file
python python/pybind11/verify_milestone1.py input.uhdm output.uhdm
```

The script will:
1. Import the `pyuhdm` module
2. Create a `UHDMDatabase` instance
3. Load the input file
4. Save to the output file (or a temporary file)
5. Verify the output file was created and is non-empty
6. Print a success/failure message

## Python Usage Example

```python
import sys
sys.path.insert(0, 'build_pybind11/lib')

import pyuhdm

# Create a database instance
db = pyuhdm.UHDMDatabase()

# Load a UHDM binary file
db.load("design.uhdm")

# Check if data was loaded
if db.is_loaded():
    print("Design loaded successfully")

# Save to a new file
db.save("design_copy.uhdm")
```

### Exception Handling

The module raises Python exceptions on errors:

```python
import pyuhdm

db = pyuhdm.UHDMDatabase()

try:
    db.load("nonexistent.uhdm")
except FileNotFoundError as e:
    print(f"File not found: {e}")

try:
    db.load("corrupted.uhdm")
except RuntimeError as e:
    print(f"Serialization error: {e}")
```

## API Reference

### `pyuhdm.UHDMDatabase`

A wrapper class that owns UHDM serializer state and provides load/save functionality.

#### Methods

| Method | Description |
|--------|-------------|
| `__init__()` | Create a new empty UHDMDatabase instance |
| `load(path: str)` | Load a UHDM binary file from disk |
| `save(path: str)` | Save the current UHDM state to a binary file |
| `is_loaded() -> bool` | Check if a design has been loaded |

#### Exceptions

- `FileNotFoundError`: Raised if a file cannot be found or opened
- `RuntimeError`: Raised if serialization/deserialization fails

## Project Structure

```
python/pybind11/
├── CMakeLists.txt        # Build configuration
├── module.cpp            # Main pybind11 module definition
├── uhdm_db.hpp           # UHDMDatabase C++ wrapper header
├── uhdm_db.cpp           # UHDMDatabase C++ wrapper implementation
├── verify_milestone1.py  # Verification script
├── README.md             # This file
├── bindings/
│   └── bind_db.cpp       # Pybind11 bindings for UHDMDatabase
└── utils/
    └── exceptions.hpp    # Custom exception types
```

## Next Steps

Future milestones will add:
- Read-only access to VPI handles
- Query methods for design hierarchy
- Expression tree traversal
