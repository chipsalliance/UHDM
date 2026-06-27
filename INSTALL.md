
## UHDM project

### Library uhdm.a

### Development Environment Required:

* Linux (Ubuntu or Centos) or Windows, cmake 3.15 and GCC/VS supporting c++17

* Please install the following package updates:

   * sudo apt-get install build-essential cmake git python3 python3-orderedmultidict

* UHDM Source code
  * git clone https://github.com/alainmarcel/UHDM.git
  * cd UHDM
  * git submodule update --init --recursive

* Build
  * make
  * make test
  * make install

### Python Bindings (Optional)

To build and use the Python bindings (`-DUHDM_WITH_PYTHON=ON`):

1. **Build in-place** (Default):
   ```bash
   mkdir -p build
   cd build
   cmake -DUHDM_WITH_PYTHON=ON ..
   make
   ```
   This compiles the SWIG module and packages it in-place under `build/python/`.

2. **Use in-place**:
   To run python scripts using the compiled module without installing it globally, set the `PYTHONPATH` environment variable:
   ```bash
   export PYTHONPATH=$(pwd)/build/python
   ```

3. **Install to active environment** (Editable mode):
   To install the package into your current python environment so it can be imported globally without `PYTHONPATH`:
   ```bash
   make PyPackageBuild
   ```

4. **Build distribution package**:
   If you want to build a distribution archive (`.tar.gz`), install the `build` package first, then run:
   ```bash
   pip install build
   make PyPackageDist
   ```
