%module uhdm
/* 
Everithing in the %{ ... %} block is simply copied verbatim to the resulting wraper file created by swig.
It is not parsed or interpreted by swig
*/
%{
#include <stdarg.h>
#include "vpi_user.h"
#include "sv_vpi_user.h"
#include "Serializer.h"
#include "ExprEval.h"
#include "vpi_visitor.h"
#include "swig_test.h"

%}
%include "std_iostream.i"
%include "std_sstream.i"
%include "typemaps.i"
%include "stdint.i"
%include "std_vector.i"

%apply bool& OUTPUT { bool &invalidValue};

/* some api function using va_list are exclude using #ifndef SWIG/#endif */
%include "vpi_user.h"
%include "sv_vpi_user.h"

%include "Serializer.h"
%include "uhdm_types.h"
%include "ExprEval.h"
%include "vpi_visitor.h"
%include "swig_test.h"

%include "uhdm_vpi_user.h"
%include "uhdm-version.h"

namespace std {
  %template(vpiHandleVector) vector<vpiHandle>;
}
