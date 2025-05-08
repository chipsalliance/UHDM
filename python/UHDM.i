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
#include "vpi_uhdm.h"
#include "RTTI.h"
#include "Serializer.h"
#include "ExprEval.h"
#include "vpi_visitor.h"
#include "swig_test.h"

#include "BaseClass.h"
#include "VpiListener.h"
#include "ElaboratorListener.h"

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
%include "vpi_uhdm.h"
%include "ExprEval.h"
%include "swig_test.h"

%include "uhdm_vpi_user.h"
%include "uhdm-version.h"

// We ignore RTTI because it's largely incompatible with base SWIG
%ignore RTTI;
class UHDM::RTTI {};
#define UHDM_IMPLEMENT_RTTI(a,b) ;
#define UHDM_IMPLEMENT_RTTI_2_BASES(a,b,c) ;
#define UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(a,b) ;
#define UHDM_IMPLEMENT_RTTI_VIRTUAL_CAST_FUNCTIONS(a,b) ;

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

%include "BaseClass.h"
%include "VpiListener.h"
%include "ElaboratorListener.h"

// SWIG doesn't seem to expose some methods to the child classes
%inline %{
    UHDM::BaseClass* UhdmBaseClassFromVpiHandle(vpiHandle hdesign) {
        if (!hdesign) return (UHDM::BaseClass*)Py_None;
        return (UHDM::BaseClass*)((uhdm_handle*)hdesign)->object;
    }
%}

namespace std {
  %template(vpiHandleVector) vector<vpiHandle>;
}

