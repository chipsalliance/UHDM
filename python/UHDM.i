%module uhdm
%{
#include <stdarg.h>
#include "sv_vpi_user.h"
#include "swig_main.h"
%}
%include "std_vector.i"


%include <sv_vpi_user.h>
/* some api function using va_list are exclude using #ifndef SWIG/#endif */
%include <vpi_user.h>

namespace UHDM {
  class Serializer{};
}

%include stl.i
namespace std {
  %template(vpiHandleVector) vector<vpiHandle>;
}

%include "swig_main.h"
