#include <string.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include <b9/Module.hpp>
#include <b9/deserialize.hpp>
#include <b9/instructions.hpp>

namespace b9 {

void readStringSection(std::istream &in, std::vector<std::string> &strings, std::shared_ptr<ModuleMmap> &moduleMmap) {
  uint32_t stringCount;
  if (!readNumber(in, stringCount)) {
    throw DeserializeException{"Error reading string count"};
  }
  moduleMmap->recordStringSection(stringCount);
  for (uint32_t i = 0; i < stringCount; i++) {
    std::string toRead;
    readString(in, toRead);
    strings.push_back(toRead);
    moduleMmap->insertStringSection(toRead, i);
  }
}

int readInstructions(std::istream &in,
                      std::vector<Instruction> &instructions,
                      std::shared_ptr<ModuleMmap> &moduleMmap) {
  int instructionCount = 0;
  do {
    RawInstruction instruction;
    if (!readNumber(in, instruction)) {
      return -1;
    }

    moduleMmap->insertInstruction(instruction);

    instructions.emplace_back(instruction);
    ++instructionCount;
  } while (instructions.back() != END_SECTION);
  return instructionCount;
}

void readFunctionData(std::istream &in, FunctionDef &functionDef, std::shared_ptr<ModuleMmap> &moduleMmap) {
  std::string temp;
  readString(in, temp);
  functionDef.name = temp;
  moduleMmap->insertNewFunc(temp);
  bool ok = readNumber(in, functionDef.nparams) &&
            readNumber(in, functionDef.nlocals);

  moduleMmap->insertParams(functionDef.nparams, functionDef.nlocals);
  moduleMmap->reserveAddrInstructions();

  if (!ok) {
    throw DeserializeException{"Error reading function data"};
  }
}

void readFunction(std::istream &in, FunctionDef &functionDef, std::shared_ptr<ModuleMmap> &moduleMmap) {
  readFunctionData(in, functionDef, moduleMmap);
  int instructionCount = readInstructions(in, functionDef.instructions, moduleMmap);
  if (instructionCount == -1) {
    throw DeserializeException{"Error reading instructions"};
  }
  moduleMmap->storeInstructionsCount(instructionCount);
}

void readFunctionSection(std::istream &in,
                         std::vector<FunctionDef> &functions,
                         std::shared_ptr<ModuleMmap> &moduleMmap) {
  uint32_t functionCount;
  if (!readNumber(in, functionCount)) {
    throw DeserializeException{"Error reading function count"};
  }
  for (uint32_t i = 0; i < functionCount; i++) {
    // TODO: Figure out a way to store FunctionDef in memory reserved by mmap
    functions.emplace_back(FunctionDef{"", std::vector<Instruction>{}, 0, 0});
    readFunction(in, functions.back(), moduleMmap);
  }
}

void readSection(std::istream &in, std::shared_ptr<Module> &module, std::shared_ptr<ModuleMmap> &moduleMmap) {
  uint32_t sectionCode;
  if (!readNumber(in, sectionCode)) {
    throw DeserializeException{"Error reading section code"};
  }

  switch (sectionCode) {
    case 1:
      return readFunctionSection(in, module->functions, moduleMmap);
    case 2:
      return readStringSection(in, module->strings, moduleMmap);
    default:
      throw DeserializeException{"Invalid Section Code"};
  }
}

std::uint32_t readModuleSize(std::istream &in) {
  if (in.peek() == std::istream::traits_type::eof()) {
    throw DeserializeException{"Empty Input File"};
  }

  std::uint32_t moduleSize;
  if (!readNumber(in, moduleSize)) {
    throw DeserializeException{"Error reading section code"};
  }

  std::cout << "From the deserializer: number read was " << moduleSize << std::endl;

  return moduleSize;
}

void readHeader(std::istream &in) {
  if (in.peek() == std::istream::traits_type::eof()) {
    throw DeserializeException{"Empty Input File"};
  }

  const char magic[] = {'b', '9', 'm', 'o', 'd', 'u', 'l', 'e'};
  const std::size_t bytes = sizeof(magic);

  char buffer[bytes];
  bool ok = readBytes(in, buffer, bytes);
  if (!ok || strncmp(magic, buffer, bytes) != 0) {
    throw DeserializeException{"Corrupt Header"};
  }
}

std::shared_ptr<Module> deserialize(std::istream &in, std::shared_ptr<ModuleMmap> &moduleMmap) {
  // TODO: mmap module
  auto module = std::make_shared<Module>();
  std::uint32_t moduleSize = readModuleSize(in);
  auto moduleMmap2 = std::make_shared<ModuleMmap>(moduleSize);

  readHeader(in);
  while (in.peek() != std::istream::traits_type::eof()) {
    readSection(in, module, moduleMmap2);
  }

  moduleMmap = moduleMmap2;
  return module;
}

}  // namespace b9
