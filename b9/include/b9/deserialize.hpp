#ifndef B9_DESERIALIZE_HPP_
#define B9_DESERIALIZE_HPP_

#include <b9/Module.hpp>
#include <b9/instructions.hpp>

#include <string.h>
#include <iostream>
#include <memory>
#include <vector>

namespace b9 {

struct DeserializeException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline bool readBytes(std::istream &in, char *buffer, long bytes) {
  long count = 0;
  do {
    in.read(&buffer[count], bytes - count);
    count += in.gcount();
  } while (count < bytes && in.good());
  return count == bytes;
}

template <typename Number>
bool readNumber(std::istream &in, Number &out, long bytes = sizeof(Number)) {
  char *buffer = (char *)&out;
  return readBytes(in, buffer, bytes);
}

void readString(std::istream &in, std::string &toRead) {
  uint32_t length;
  if (!readNumber(in, length, sizeof(length))) {
    throw DeserializeException{"Error reading string length"};
  }
  for (size_t i = 0; i < length; i++) {
    if (in.eof()) {
      throw DeserializeException{"Error reading string"};
    }
    char current = in.get();
    toRead.push_back(current);
  }
}

void readStringSection(std::istream &in, std::vector<std::string> &strings, std::shared_ptr<ModuleMmap> &module);

int readInstructions(std::istream &in, std::vector<Instruction> &instructions, std::shared_ptr<ModuleMmap> &module);

void readFunctionData(std::istream &in, FunctionDef &functionSpec, std::shared_ptr<ModuleMmap> &module);

void readFunction(std::istream &in, FunctionDef &functionDef);

void readFunctionSection(std::istream &in, std::vector<FunctionDef> &functions, std::shared_ptr<ModuleMmap> &module);

void readSection(std::istream &in, std::shared_ptr<Module> &module);

// void readModuleSize(std::istream &in);

void readHeader(std::istream &in, char *buffer);

std::shared_ptr<Module> deserialize(std::istream &in, std::shared_ptr<ModuleMmap> &module); // TODO: Get rid of extra argument and return proper module

}  // namespace b9

#endif  // B9_DESERIALIZE_HPP_
