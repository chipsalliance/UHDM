#include "headers/uhdm.h"
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <dirent.h>
#include <stdio.h>
#include <regex>
#include <fstream>
#include <sstream>

using namespace UHDM;

#include "test_helper.h"

int main (int argc, char** argv) {
  std::string fileName = "surelog.uhdm";
  if (argc > 1) {
    fileName = argv[1];
  }
  struct stat buffer;
  if (stat(fileName.c_str(), &buffer) != 0) {
      std::cout << "File " << fileName << " does not exist!" << std::endl;
      return 1;
  }
  std::cout << "Restore design from: " << fileName << std::endl;
  std::vector<vpiHandle> restoredDesigns = Serializer::Restore(fileName);
  
  std::string restored = print_designs(restoredDesigns);
  std::cout << restored;
  return 0;
};
