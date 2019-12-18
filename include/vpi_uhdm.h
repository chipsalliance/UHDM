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
  uhdm_handle(unsigned int type, const void* object) :
    type(type), object(object), index(0) {}

  const unsigned int type;
  const void* object;
  unsigned int index;
};

#endif
