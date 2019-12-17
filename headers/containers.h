#ifndef CONTAINER_H
#define CONTAINER_H
namespace UHDM {
class module;
typedef std::vector<module*> VectorOfmodule;
typedef std::vector<module*>* VectorOfmodulePtr;
typedef std::vector<module*>& VectorOfmoduleRef;
typedef std::vector<module*>::iterator VectorOfmoduleItr;
};
#endif
