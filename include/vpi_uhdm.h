// -*- c++ -*-

#ifndef VPI_UHDM_H
#define VPI_UHDM_H

namespace UHDM {

  class BaseClass {
  public: 
    BaseClass(){}
    virtual ~BaseClass(){}
  };

  class Serializer {
  public:
    static void save(std::string file);
    static const std::vector<vpiHandle> restore(std::string file);
  };

};

struct uhdm_handle {
  uhdm_handle(unsigned int type, const void* object) :
    type(type), object(object), index(0) {}

  const unsigned int type;
  const void* object;
  unsigned int index;
};

class uhdm_handleFactory {
  friend UHDM::Serializer;
  public:
  static vpiHandle make(unsigned int type, const void* object) {
    uhdm_handle* obj = new uhdm_handle(type, object);
    objects_.push_back(obj);
    return (vpiHandle) obj;
  }
  private:
    static std::vector<uhdm_handle*> objects_;
  };

#endif
