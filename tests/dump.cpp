#include <iostream>
#include <sys/stat.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <regex>
#include <fstream>
#include <sstream>

#if !(defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__))
  #include <dirent.h>
  #include <unistd.h>
#endif

#include "headers/uhdm.h"
#include "headers/vpi_listener.h"
#include "headers/vpi_visitor.h"
#include "headers/ElaboratorListener.h"

using namespace UHDM;

int main (int argc, char** argv) {
  std::string fileName = "surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }

  bool elab = false;
  if (argc > 2) {
    if (std::string(argv[2]) == "-elab") {
      elab = true;
    }
  }
  
  struct stat buffer;
  if (stat(fileName.c_str(), &buffer) != 0) {
      std::cout << "File " << fileName << " does not exist!" << std::endl;
      return 1;
  }
  Serializer serializer;
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore(fileName);
  
  std::string restored = visit_designs(restoredDesigns);
  std::cout << restored;

  if (elab) {
    ElaboratorListener* listener = new ElaboratorListener(&serializer, false);
    listen_designs(restoredDesigns, listener);
    std::string restoredPostElab = visit_designs(restoredDesigns);
    std::cout << "Restore design Post-Elab: " << std::endl;
    std::cout << restoredPostElab;
  }
  
  return 0;
};
