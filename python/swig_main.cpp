#include "swig_main.h"

const std::vector<vpiHandle> read_uhdm(UHDM::Serializer * s, std::string filename){
  return (s->Restore(filename));
}
