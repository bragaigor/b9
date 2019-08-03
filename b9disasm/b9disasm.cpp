#include <fstream>
#include <iostream>

#include <b9/Module.hpp>
#include <b9/deserialize.hpp>

using namespace b9;

extern "C" int main(int argc, char** argv) {
  std::ifstream infile;
  std::streambuf* inbuffer = nullptr;

  if (argc == 1) {
    inbuffer = std::cin.rdbuf();
  } else {
    infile.open(argv[1], std::ios::in | std::ios::binary);
    inbuffer = infile.rdbuf();
  }

  std::istream in(inbuffer);
  
  std::shared_ptr<ModuleMmap> moduleMmap;
  auto module = deserialize(in, moduleMmap);
  std::cout << *module;
  std::cout << "\n*******************************************************************\n";
  std::cout << *moduleMmap;
}
