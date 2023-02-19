#include "swig_main.h"

std::vector<vpiHandle> read_uhdm(std::string filename){
  UHDM::Serializer serializer;
  return serializer.Restore(filename);
}


