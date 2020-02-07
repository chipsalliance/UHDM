++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
## UHDM project
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
### Library uhdm.a
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

### Development Environment Required:

* Linux (Ubuntu or Centos)

* Please install the following package updates:

   * sudo apt-get install build-essential cmake git tclsh

* UHDM Source code
  * git clone https://github.com/alainmarcel/UHDM.git
  * cd UHDM
  * git submodule update --init --recursive
  * Remove capnproto limits:
  * sed -i 's/nestingLimit = 64/nestingLimit = 1024/g' third_party/capnproto/c++/src/capnp/message.h
  * sed -i 's/8 \* 1024 \* 1024/64 \* 1024 \* 1024 \* 1024/g' third_party/capnproto/c++/src/capnp/message.h 
* Build
  * make
  * make test
  * make install
