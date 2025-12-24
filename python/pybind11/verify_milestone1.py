#!/usr/bin/env python3
"""
Milestone 1 Verification Script for pyuhdm

This script performs a simple sanity check of the pyuhdm pybind11 module by:
1. Creating a UHDMDatabase instance
2. Loading an existing .uhdm file
3. Saving it to a new output file
4. Verifying the output file is created and non-empty

Usage:
    python verify_milestone1.py <input.uhdm> [output.uhdm]

If output path is not provided, a temporary file will be used.
"""

import sys
import os
import tempfile


def find_module_path():
    """Find and add the pyuhdm module path."""
    # Try common build directories
    script_dir = os.path.dirname(os.path.abspath(__file__))
    possible_paths = [
        os.path.join(script_dir, '..', '..', 'build_pybind11', 'lib'),
        os.path.join(script_dir, '..', '..', 'build', 'lib'),
        os.path.join(script_dir, '..', '..', 'build_pybind11', 'python', 'pybind11'),
    ]
    
    for path in possible_paths:
        abs_path = os.path.abspath(path)
        if os.path.isdir(abs_path):
            if abs_path not in sys.path:
                sys.path.insert(0, abs_path)
            return abs_path
    
    return None


def main():
    if len(sys.argv) < 2:
        print("Usage: python verify_milestone1.py <input.uhdm> [output.uhdm]")
        print("\nThis script verifies the pyuhdm module's load/save functionality.")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    
    # Validate input file exists
    if not os.path.isfile(input_path):
        print(f"[FAIL] Input file not found: {input_path}")
        sys.exit(1)
    
    # Find and setup module path
    module_path = find_module_path()
    if module_path:
        print(f"[INFO] Using module path: {module_path}")
    
    # Import the module
    try:
        import pyuhdm
        print("[OK] Successfully imported pyuhdm module")
    except ImportError as e:
        print(f"[FAIL] Could not import pyuhdm: {e}")
        print("\nMake sure the module is built. Run:")
        print("  cd <UHDM_ROOT>")
        print("  cmake -S . -B build_pybind11")
        print("  cmake --build build_pybind11 --target pyuhdm")
        sys.exit(1)
    
    # Create UHDMDatabase instance
    try:
        db = pyuhdm.UHDMDatabase()
        print("[OK] Created UHDMDatabase instance")
    except Exception as e:
        print(f"[FAIL] Could not create UHDMDatabase: {e}")
        sys.exit(1)
    
    # Load the input file
    try:
        db.load(input_path)
        print(f"[OK] Loaded UHDM file: {input_path}")
    except FileNotFoundError as e:
        print(f"[FAIL] File not found error during load: {e}")
        sys.exit(1)
    except RuntimeError as e:
        print(f"[FAIL] Runtime error during load: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"[FAIL] Unexpected error during load: {e}")
        sys.exit(1)
    
    # Check if loaded
    try:
        if db.is_loaded():
            print("[OK] Database reports design is loaded")
        else:
            print("[WARN] Database reports no design loaded (file may be empty)")
    except Exception as e:
        print(f"[WARN] Could not check is_loaded(): {e}")
    
    # Determine output path
    use_temp = output_path is None
    if use_temp:
        fd, output_path = tempfile.mkstemp(suffix='.uhdm')
        os.close(fd)
        print(f"[INFO] Using temporary output file: {output_path}")
    
    # Save to output file
    try:
        db.save(output_path)
        print(f"[OK] Saved UHDM file: {output_path}")
    except FileNotFoundError as e:
        print(f"[FAIL] File error during save: {e}")
        sys.exit(1)
    except RuntimeError as e:
        print(f"[FAIL] Runtime error during save: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"[FAIL] Unexpected error during save: {e}")
        sys.exit(1)
    
    # Verify output file exists and is non-empty
    if os.path.isfile(output_path):
        size = os.path.getsize(output_path)
        if size > 0:
            print(f"[OK] Output file exists and has size: {size} bytes")
        else:
            print("[WARN] Output file exists but is empty (0 bytes)")
    else:
        print(f"[FAIL] Output file was not created: {output_path}")
        sys.exit(1)
    
    # Cleanup temporary file
    if use_temp:
        try:
            os.remove(output_path)
            print(f"[INFO] Cleaned up temporary file")
        except OSError:
            pass
    
    print("\n" + "=" * 50)
    print("[SUCCESS] Milestone 1 verification completed!")
    print("=" * 50)
    print("\nThe pyuhdm module can successfully:")
    print("  - Create a UHDMDatabase instance")
    print("  - Load a .uhdm file")
    print("  - Save to a .uhdm file")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
