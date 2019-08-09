#include <string.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include <b9/Module.hpp>
#include <b9/instructions.hpp>
#include <b9/serialize.hpp>

namespace b9 {

void writeStringSection(std::ostream &out,
                        const std::vector<std::string> &strings,
                        std::shared_ptr<ModuleMmap> &moduleMmap) {

  void *stringSection = moduleMmap->getStringSection();
  std::uint32_t strCount = *((instruction_type*)(stringSection));
  if (!writeNumber(out, strCount)) {
    throw SerializeException("Error writing string section");
  }

  for (size_t i = 0; i < strCount; i++) {
    const char *currStr = moduleMmap->getString(i);
    writeString(out, std::string(currStr));
  }
  

  // uint32_t stringCount = strings.size();
  // if (!writeNumber(out, stringCount)) {
  //   throw SerializeException("Error writing string section");
  // }
  // for (auto string : strings) {
  //   writeString(out, string);
  // }
}

bool writeInstructions(std::ostream &out,
                       const std::vector<Instruction> &instructions,
                       std::shared_ptr<ModuleMmap> &moduleMmap,
                       void *funcPtr,
                       size_t funcNameLength) {

  uint32_t instructionCount = moduleMmap->getNextInt32Val(); // This holds instruction count
  std::uint32_t *thisInstruction = moduleMmap->getCurrentInstruction(funcPtr, funcNameLength + INSTRUCTION_SIZE*4);

  for (size_t i = 0; i < instructionCount; i++)
  {
    if (!writeNumber(out, *thisInstruction)) {
      return false;
    }
    thisInstruction++;
  }
  
  // for (auto instruction : instructions) {
  //   if (!writeNumber(out, instruction)) {
  //     return false;
  //   }
  // }
  return true;
}

void *writeFunctionData(std::ostream &out, 
                       const FunctionDef &functionDef, 
                       std::shared_ptr<ModuleMmap> &moduleMmap,
                       size_t index,
                       size_t &funcNameLength) {

  void *funcPtr = moduleMmap->getFunction(index); 
  std::string funcName =  moduleMmap->getFunctionName(funcPtr);
  std::uint32_t nparams = moduleMmap->getFunctionNparams(funcPtr, funcName.size() + INSTRUCTION_SIZE);
  std::uint32_t nlocals = moduleMmap->getFunctionNLocals(funcPtr, funcName.size() + INSTRUCTION_SIZE*2);
  funcNameLength = funcName.size();
  
  writeString(out, funcName);
  bool ok = writeNumber(out, nparams) &&
            writeNumber(out, nlocals);
  if (!ok) {
    throw SerializeException("Error writing function data");
  }

  return funcPtr;
}

void writeFunction(std::ostream &out, 
                   const FunctionDef &functionDef, 
                   std::shared_ptr<ModuleMmap> &moduleMmap,
                   size_t index) {
  size_t funcNameLenght = 0;
  // TODO: Find a better way to write functions data 
  void *funcPtr = writeFunctionData(out, functionDef, moduleMmap, index, funcNameLenght);
  if (funcNameLenght <= 0) {
    throw SerializeException("Error writing instructions");
  }
  if (!writeInstructions(out, functionDef.instructions, moduleMmap, funcPtr, funcNameLenght)) {
    throw SerializeException("Error writing instructions");
  }
}

void writeFunctionSection(std::ostream &out,
                          const std::vector<FunctionDef> &functions, 
                          std::shared_ptr<ModuleMmap> &moduleMmap) {
  uint32_t functionCount = functions.size();
  uint32_t functionCount2 = moduleMmap->getNumberOfFunctions();
  std::cout << "Writing true function count :::: " << functionCount << " :: mine: " << functionCount << std::endl;
  if (!writeNumber(out, functionCount2)) {
    throw SerializeException("Error writing function count");
  }
  // for (auto function : functions) {
  //   writeFunction(out, function);
  // }
  for (size_t i = 0; i < functionCount2; i++)
  {
    writeFunction(out, functions[i], moduleMmap, i);
  }
  
}

void writeSections(std::ostream &out, const Module &module, std::shared_ptr<ModuleMmap> &moduleMmap) {
  // if (module.functions.size() != 0) {
    if (moduleMmap->getNumberOfFunctions() != 0) {
      uint32_t sectionCode = 1;
      if (!writeNumber(out, sectionCode)) {
        throw SerializeException("Error writing function section code");
      }
      writeFunctionSection(out, module.functions, moduleMmap);
    }
  // }

  if (moduleMmap->hasStringSection()) {
  // if (module.strings.size() != 0) {
    uint32_t sectionCode = 2;
    if (!writeNumber(out, sectionCode)) {
      throw SerializeException("Error writing string section code");
    }
    writeStringSection(out, module.strings, moduleMmap);
  }
}

void writeHeader(std::ostream &out) {
  const char header[] = {'b', '9', 'm', 'o', 'd', 'u', 'l', 'e'};
  uint32_t length = 8;
  out.write(header, length);
  if (!out.good()) {
    throw SerializeException("Error writing header");
  }
}

void writeModuleSize(std::ostream &out, uint32_t moduleSize) {
  if (!writeNumber(out, moduleSize)) {
    throw SerializeException("Error writing module size");
  }
}

void serialize(std::ostream &out, const Module &module, std::shared_ptr<ModuleMmap> &moduleMmap) {
  writeModuleSize(out, 256); // TODO: Pass proper module size
  writeHeader(out);
  writeSections(out, module, moduleMmap);
}

}  // namespace b9
