// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#ifndef UHDM_TEST_HELPER_H
#define UHDM_TEST_HELPER_H

#include <string>
#include <vector>

#include <uhdm/uhdm_types.h>
#include <uhdm/sv_vpi_user.h>

std::string print_designs (const std::vector<vpiHandle>& designs) {
  std::string result;
  for (auto restoredDesign : designs) {
    result += "Design name: " + std::string(vpi_get_str(vpiName, restoredDesign)) + "\n";

    // VPI test
    vpiHandle modItr = vpi_iterate(UHDM::uhdmallModules, restoredDesign);
    while (vpiHandle obj_h = vpi_scan(modItr) ) {
      if (vpi_get(vpiType, obj_h) != vpiModule) {
        result += "ERROR: this is not a module\n";
      }
      std::string defName;
      std::string objectName;
      if (const char* s = vpi_get_str(vpiDefName, obj_h)) {
        defName = s;
      }
      if (const char* s = vpi_get_str(vpiName, obj_h)) {
        if (defName != "") {
          defName += " ";
        }
        objectName = std::string("(") + s + std::string(")");
      }
      result +=  "+ module: " + defName + objectName 
        + ", top:"
        + std::to_string(vpi_get(vpiTopModule, obj_h))
        + ", parent:" + std::string(vpi_get_str(vpiDefName, vpi_handle(vpiParent, obj_h)))
        + ", file:" +  std::string(vpi_get_str(vpiFile, obj_h))
        + ", line:" + std::to_string(vpi_get(vpiLineNo, obj_h));
      vpiHandle submodItr = vpi_iterate(vpiModule, obj_h);
      while (vpiHandle sub_h = vpi_scan(submodItr) ) {
        result += "\n    \\_ mod:" + std::string(vpi_get_str(vpiDefName, sub_h))
          + ", top:" + std::to_string(vpi_get(vpiTopModule, sub_h))
          + ", parent:" + std::string(vpi_get_str(vpiDefName, vpi_handle(vpiParent, sub_h)))
          + ", file:" +  std::string(vpi_get_str(vpiFile, sub_h))
          + ", line:" + std::to_string(vpi_get(vpiLineNo, sub_h));
        vpi_release_handle (sub_h);
      }
      vpiHandle importItr = vpi_iterate(vpiImport, obj_h);
      while (vpiHandle sub_h = vpi_scan(importItr) ) {
        result += "\n    \\_ inst_item:" + std::string(vpi_get_str(vpiDefName, sub_h));
        if (vpiFunction == vpi_get(vpiType, sub_h))
          result += std::string(", size:") + std::to_string(vpi_get(vpiSize, sub_h));
      }
      vpi_release_handle (submodItr);
      result += "\n";
      vpi_release_handle (obj_h);
    }
    vpi_release_handle(modItr);

    vpiHandle pacItr = vpi_iterate(UHDM::uhdmallPackages, restoredDesign);
    while (vpiHandle obj_h = vpi_scan(pacItr) ) {
      if (vpi_get(vpiType, obj_h) != vpiPackage) {
        result += "ERROR: this is not a package\n";
      }
      result +=  "+ package: " + std::string(vpi_get_str(vpiName, obj_h)) + "\n";
      vpiHandle funcItr = vpi_iterate(vpiTaskFunc, obj_h);
      while (vpiHandle func_h = vpi_scan(funcItr) ) {
        result +=  "  func: " + std::string(vpi_get_str(vpiName, func_h)) + std::string(", size:") + std::to_string(vpi_get(vpiSize, func_h));
        result += "\n";
      }
    }
  }
  return result;
}

#endif  // UHDM_TEST_HELPER_H
