

std::string print_designs (std::vector<vpiHandle> designs) {
  std::string result;
  for (auto restoredDesign : designs) {
    result += "Design name: " + std::string(vpi_get_str(vpiName, restoredDesign)) + "\n";
    
    // VPI test
    vpiHandle modItr = vpi_iterate(uhdmallModules,restoredDesign); 
    while (vpiHandle obj_h = vpi_scan(modItr) ) {
      if (vpi_get(vpiType, obj_h) != vpiModule) {
	exit (1);
      }
      result +=  "mod:" + std::string(vpi_get_str(vpiName, obj_h))
	+ ", top:" 
	+ std::to_string(vpi_get(vpiTopModule, obj_h))
	+ ", parent:" + std::string(vpi_get_str(vpiName, vpi_handle(vpiParent, obj_h)))
        + ", file:" +  std::string(vpi_get_str(vpiFile, obj_h))
	+ ", line:" + std::to_string(vpi_get(vpiLineNo, obj_h));
      vpiHandle submodItr = vpi_iterate(vpiModule, obj_h); 
      while (vpiHandle sub_h = vpi_scan(submodItr) ) {
	result += "\n  \\_ mod:" + std::string(vpi_get_str(vpiName, sub_h)) 
	  + ", top:" + std::to_string(vpi_get(vpiTopModule, sub_h))
	  + ", parent:" + std::string(vpi_get_str(vpiName, vpi_handle(vpiParent, sub_h)))
	  + ", file:" +  std::string(vpi_get_str(vpiFile, sub_h))
	  + ", line:" + std::to_string(vpi_get(vpiLineNo, sub_h));
	vpi_release_handle (sub_h);
      }
      vpi_release_handle (submodItr);
    
    result += "\n";
    vpi_release_handle (obj_h);
    }
    vpi_release_handle(modItr);
  }
  return result;
}

