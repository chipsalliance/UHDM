%module uhdm
/* 
Everithing in the %{ ... %} block is simply copied verbatim to the resulting wraper file created by swig.
It is not parsed or interpreted by swig
*/
%{
#include <stdarg.h>
#include "vpi_user.h"
#include "sv_vpi_user.h"
#include "uhdm_vpi_user.h"
#include "Serializer.h"
#include "BaseClass.h"

#include "uhdm.h"

%}
%include "std_vector.i"
%include "std_string.i"

/* some api function using va_list are exclude using #ifndef SWIG/#endif */
%include "vpi_user.h"
%include "sv_vpi_user.h"
%include "uhdm_vpi_user.h"

%include "Serializer.h"
%include "BaseClass.i"

%include "uhdm_swig_type.i"


%include stl.i
namespace std {
  %template(vpiHandleVector) vector<vpiHandle>;
}

%insert("header") %{
using namespace UHDM;
%}

