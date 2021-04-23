
## UHDM project

### Library uhdm.a


### Development Environment Required:

* Linux (Ubuntu or Centos) or Windows, cmake 3.15 and GCC/VS supporting c++17

* Please install the following package updates:

   * sudo apt-get install build-essential cmake git tclsh

* UHDM Source code
  * git clone https://github.com/alainmarcel/UHDM.git
  * cd UHDM
  * git submodule update --init --recursive
  * cd third_party/capnproto
  * git reset --hard d4a3770

* Build
  * make
  * make test
  * make install
