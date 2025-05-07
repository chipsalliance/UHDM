%module uhdm
/* 
Everithing in the %{ ... %} block is simply copied verbatim to the resulting wrapper file created by swig.
It is not parsed or interpreted by swig
*/
%{
#include <stdarg.h>
#include <sstream>
#include <vector>

#include "uhdm_forward_decl.h"
#include "vpi_user.h"
#include "sv_vpi_user.h"

#include "uhdm_types.h"
#include "RTTI.h"
#include "Serializer.h"
#include "VpiListener.h"
#include "ElaboratorListener.h"
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
%include "uhdm_forward_decl.h"
%include "vpi_user.h"
%include "sv_vpi_user.h"

%include "uhdm_types.h"
%include "VpiListener.h"
%include "ElaboratorListener.h"
%include "ExprEval.h"
%include "swig_test.h"

%include "uhdm_vpi_user.h"
%include "uhdm-version.h"

// PrintStats requires a C++ output stream, here we convert to a Pythonic string
%ignore UHDM::Serializer::PrintStats;
%include "Serializer.h"
%extend UHDM::Serializer {
  std::string GetStats(const std::string& infoText) {
    std::ostringstream oss;
    $self->PrintStats(oss, infoText);
    return oss.str();
  }
}

// visit_designs requires a C++ output stream, here we convert to a Pythonic string
%ignore UHDM::visit_designs;
%include "vpi_visitor.h"
%inline %{
  std::string visit_designs(const std::vector<vpiHandle>& designs) {
    std::ostringstream oss;
    UHDM::visit_designs(designs, oss);
    return oss.str();
  }
%}

namespace std {
  %template(vpiHandleVector) vector<vpiHandle>;
}

