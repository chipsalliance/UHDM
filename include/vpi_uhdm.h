// -*- c++ -*-

#ifndef VPI_UHDM_H
#define VPI_UHDM_H

namespace UHDM {
class BaseClass {
public:
  BaseClass(){}
  virtual ~BaseClass(){}
};
};


struct uhdm_handle {
  uhdm_handle(unsigned int type, void* object) :
    type(type), object(object), index(0) {}

  unsigned int type;
  void* object;
  unsigned int index;
};

#endif
