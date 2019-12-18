#ifndef CONTAINER_H
#define CONTAINER_H
namespace UHDM {
class port;
typedef std::vector<port*> VectorOfport;
typedef std::vector<port*>::iterator VectorOfportItr;
class interface;
typedef std::vector<interface*> VectorOfinterface;
typedef std::vector<interface*>::iterator VectorOfinterfaceItr;
class interface_array;
typedef std::vector<interface_array*> VectorOfinterface_array;
typedef std::vector<interface_array*>::iterator VectorOfinterface_arrayItr;
class cont_assign;
typedef std::vector<cont_assign*> VectorOfcont_assign;
typedef std::vector<cont_assign*>::iterator VectorOfcont_assignItr;
class module;
typedef std::vector<module*> VectorOfmodule;
typedef std::vector<module*>::iterator VectorOfmoduleItr;
class module_array;
typedef std::vector<module_array*> VectorOfmodule_array;
typedef std::vector<module_array*>::iterator VectorOfmodule_arrayItr;
class mod_path;
typedef std::vector<mod_path*> VectorOfmod_path;
typedef std::vector<mod_path*>::iterator VectorOfmod_pathItr;
class tchk;
typedef std::vector<tchk*> VectorOftchk;
typedef std::vector<tchk*>::iterator VectorOftchkItr;
class io_decl;
typedef std::vector<io_decl*> VectorOfio_decl;
typedef std::vector<io_decl*>::iterator VectorOfio_declItr;
};
#endif
