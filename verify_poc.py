import sys
import os

# Adjust path to where the .so file is generated
# CMake usually puts it in the binary dir, possibly under a subdirectory or proper layout
# For now, we'll try to find it or assume it's in build_poc/python/poc_pybind11

sys.path.append(os.path.abspath("build_poc/lib"))

try:
    import uhdm_py
    print(f"Doc: {uhdm_py.__doc__}")
    print(f"Hello: {uhdm_py.hello()}")
    
    test_str = "  foo bar  "
    trimmed = uhdm_py.trim_string(test_str)
    print(f"Trim: '{test_str}' -> '{trimmed}'")
    assert trimmed == "foo bar", f"Expected 'foo bar', got '{trimmed}'"
except ImportError as e:
    print(f"ImportError: {e}")
    sys.exit(1)
