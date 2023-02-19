%module uhdm
%{
#include "sv_vpi_user.h"
%}
%include "std_vector.i"
%include <sv_vpi_user.h>

%template(vpiHandleVector) std::vector<vpiHandle>;
