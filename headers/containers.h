#ifndef CONTAINER_H
#define CONTAINER_H
namespace UHDM {
#define moduleID 500
#define designID 507
class module;
typedef std::vector<module*> VectorOfmodule;
typedef std::vector<module*>& VectorOfmoduleRef;
typedef std::vector<module*>::iterator VectorOfmoduleItr;
#define allModules 508
#define topModules 509
};
#endif
