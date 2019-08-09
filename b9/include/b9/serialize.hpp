#ifndef B9_SERIALIZE_HPP_
#define B9_SERIALIZE_HPP_

#include <b9/Module.hpp>
#include <fstream>
#include <iostream>

namespace b9 {

struct SerializeException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

template <typename Number>
bool writeNumber(std::ostream &out, const Number &n) {
  const long bytes = sizeof(Number);
  auto buffer = reinterpret_cast<const char *>(&n);
  out.write(buffer, bytes);
  return out.good();
}

inline void writeString(std::ostream &out, std::string toWrite) {
  if (toWrite[toWrite.size() - 1] == '\0') {
    toWrite.pop_back(); // Pops null character!!!
  }
  uint32_t length = toWrite.length();
  if (!writeNumber(out, length)) {
    throw SerializeException{"Error writing string length"};
  }
  out << toWrite;
  if (!out.good()) {
    throw SerializeException{"Error writing string"};
  }
}

void writeStringSection(std::ostream &out,
                        const std::vector<std::string> &strings,
                        std::shared_ptr<ModuleMmap> &moduleMmap);

bool writeInstructions(std::ostream &out,
                       const std::vector<Instruction> &instructions,
                       std::shared_ptr<ModuleMmap> &moduleMmap,
                       void *funcPtr,
                       size_t funcNameLength);

void *writeFunctionData(std::ostream &out, 
                       const FunctionDef &functionDef, 
                       std::shared_ptr<ModuleMmap> &moduleMmap,
                       size_t index,
                       size_t &funcNameLength);

void writeFunction(std::ostream &out, 
                   const FunctionDef &functionDef, 
                   std::shared_ptr<ModuleMmap> &moduleMmap,
                   size_t index);

void writeFunctionSection(std::ostream &out,
                          const std::vector<FunctionDef> &functions, 
                          std::shared_ptr<ModuleMmap> &moduleMmap);

void writeSections(std::ostream &out, const Module &module, std::shared_ptr<ModuleMmap> &moduleMmap);

void writeHeader(std::ostream &out);

void serialize(std::ostream &out, const Module &module, std::shared_ptr<ModuleMmap> &moduleMmap);

}  // namespace b9

#endif  // B9_SERIALIZE_HPP_
