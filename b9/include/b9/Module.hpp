#if !defined(B9_MODULE_HPP_)
#define B9_MODULE_HPP_

#include <b9/instructions.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <sys/mman.h>
#include <sstream>

namespace b9 {

class Compiler;
class ExecutionContext;
class VirtualMachine;

struct ModuleException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Function Definition

struct FunctionDef {
  std::string name;
  std::vector<Instruction> instructions;
  std::uint32_t nparams;
  std::uint32_t nlocals;
};

class ModuleMmap {

  public:

  ~ModuleMmap() {
    if (_currentPtr != NULL) {
      munmap(_currentPtr, _size);
    }
  }

  ModuleMmap(std::uint32_t size) : _size(size) {

    int protFlags = PROT_WRITE | PROT_READ;
    int flags = MAP_PRIVATE | MAP_ANON;

    void* addr = mmap(NULL, 
                      size, 
                      protFlags, 
                      flags, 
                      -1, 
                      0);

    if (addr == NULL) {
      std::cout << "mmap() failed. Generate proper error and exit.\n";
      // TODO: Proper error handling?
      throw ModuleException{"Error when trying to mmap Module"};
      
    } else {
      _currentPtr = addr;
      std::cout << "mmap() successfully.\n";
    }


  }

  void *getFunction(std::size_t index) {
    if (index >= funcAddrs_.size()) {
      std::stringstream ss;
      ss << "Error trying to find index " << index << " in map.";
      std::string message = ss.str();
      throw ModuleException{message};
    }
    _currentPtr = funcAddrs_[index];

    return _currentPtr;
  }

  std::string getFunctionName(void *funcPtr) {

    checkAddress(funcPtr);
    std::string toReturn;
    std::uint32_t stringN = *((instruction_type*)(_currentPtr));
    _currentPtr = static_cast<instruction_type*>(_currentPtr) + 1;
    char* backStr = static_cast<char*>(_currentPtr);

    for (int i = 0; i < stringN; i++) {
        toReturn.push_back(*backStr);
        backStr += sizeof(char);
    }
    
    _currentPtr = static_cast<char*>(_currentPtr) + (sizeof(char) * stringN);
    std::cout << "Inside Module::getFunctionName() string length: " << stringN 
                              << ", string read: " << toReturn << std::endl;
    return toReturn;
  }

  std::uint32_t getFunctionNparams(void *funcPtr, std::uint32_t offset) {
    checkAddress((void*)((size_t)funcPtr + offset));

    return getNextInt32Val();
  }

  std::uint32_t getFunctionNLocals(void *funcPtr, std::uint32_t offset) {
    checkAddress((void*)((size_t)funcPtr + offset));

    return getNextInt32Val();
  }

  std::uint32_t *getCurrentInstruction(void *funcPtr, std::uint32_t offset) {

    checkAddress((void*)((size_t)funcPtr + offset));

	  std::uint32_t *toReturn = ((instruction_type *)_currentPtr);
	  _currentPtr = static_cast<instruction_type*>(_currentPtr) + 1; // TODO: Might not need this
	  return toReturn;
  }

  const char *getString(int index) {
    void *indexPtr = static_cast<uintptr_t*>(_strSection) + index;
    uintptr_t func = *((uintptr_t*)(indexPtr));
    void *funcPtr = (void*)func;
    funcPtr = static_cast<instruction_type*>(funcPtr) + 1; // Jump function length
    const char *strPtr = static_cast<const char *>(funcPtr);
    
    return strPtr;
  }

  void recordStringSection(std::uint32_t stringCount) {
    _strSection = _currentPtr;

    // Reserve placeholders in mmap to hold addresses to strings 
    for (int i = 0; i < stringCount; i++)
    {
      *static_cast<uintptr_t*>(_currentPtr) = 0; 
      _currentPtr = static_cast<uintptr_t*>(_currentPtr) + 1;
    }
    
  }

  bool insertNewFunc(std::string &functionName) {
    std::cout << "Inside insertNewFunc(): insert idx: " << funcAddrs_.size() << ", with val: " 
                              << functionName << ", at address: " << _currentPtr << std::endl;
    std::cout << "Function name length: " << functionName.length() << ", function name: " 
              << functionName << std::endl;

    funcAddrs_.push_back(_currentPtr);
    insertString(functionName);
    return true;
  }

  bool insertStringSection(std::string str, std::uint32_t index) {
    strings.push_back(str);

    void *strPtr = insertString(str);
    void *indexPtr = static_cast<uintptr_t*>(_strSection) + index;
    *static_cast<uintptr_t*>(indexPtr) = (uintptr_t)strPtr;

    return true;
  }

  void *insertString(std::string str) {
    std::uint32_t funcLength = str.length();
    void *strPtr = _currentPtr;
    *static_cast<instruction_type*>(_currentPtr) = funcLength+1; 
    _currentPtr = static_cast<instruction_type*>(_currentPtr) + 1;

    std::memcpy(_currentPtr, str.c_str(), funcLength);

    _currentPtr = static_cast<char*>(_currentPtr) + funcLength;
    std::memcpy(_currentPtr, "\0", 1);

    _currentPtr = static_cast<char*>(_currentPtr) + 1;

    return strPtr;
  }

  bool insertParams(std::uint32_t nparams, std::uint32_t nlocals) {
    *static_cast<std::uint32_t*>(_currentPtr) = nparams;
    _currentPtr = static_cast<instruction_type*>(_currentPtr) + 1;
    *static_cast<std::uint32_t*>(_currentPtr) = nlocals;
    _currentPtr = static_cast<instruction_type*>(_currentPtr) + 1;

    return true;
  }

  bool insertInstruction(instruction_type instruction) {
    std::cout << "Inserting instruction 0x" << std::hex << instruction << std::dec 
              << ", at address: " << _currentPtr << std::endl;

    *static_cast<instruction_type*>(_currentPtr) = instruction;
    _currentPtr = static_cast<instruction_type*>(_currentPtr) + 1;

    return true;
  }

  std::uint32_t getNumberOfFunctions() {
    return funcAddrs_.size();
  }

  std::uint32_t getNextInt32Val() {

    std::uint32_t myNumber = *((instruction_type*)(_currentPtr));
    std::cout << "Inside getNextInt32Val(), returning num: 0x" << std::hex << myNumber 
              << std::dec << ", at address: " << _currentPtr << std::endl;

    _currentPtr = static_cast<instruction_type*>(_currentPtr) + 1;

    return myNumber;
  }

  std::vector<std::string> strings;

  private:

  void checkAddress(void *funcPtr) {

    if ((size_t)funcPtr != (size_t)_currentPtr) {
      std::stringstream ss;
      ss << "Functions should point to the same address. Param: " << funcPtr 
          << ", local address: " << _currentPtr;
      std::string message = ss.str();
      throw ModuleException{message};
    }
  }

  std::vector<void*> funcAddrs_;
  void* _currentPtr;
  void* _strSection;
  std::uint32_t _size;
};

inline void operator<<(std::ostream& out, const FunctionDef& f) {
  out << "(function \"" << f.name << "\" " << f.nparams << " " << f.nlocals;
  std::size_t i = 0;
  for (auto instruction : f.instructions) {
    out << std::endl << "  " << i << "  " << instruction;
    i++;
  }
  out << ")" << std::endl << std::endl;
}

inline bool operator==(const FunctionDef& lhs, const FunctionDef& rhs) {
  return lhs.name == rhs.name && lhs.nparams == rhs.nparams &&
         lhs.nlocals == rhs.nlocals;
}

/// Function not found exception.
struct FunctionNotFoundException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Primitive Function from Interpreter call
extern "C" typedef void(PrimitiveFunction)(ExecutionContext* context);

/// An interpreter module.
struct Module {
  std::vector<FunctionDef> functions;
  std::vector<std::string> strings;

  std::size_t getFunctionIndex(const std::string& name) const {
    for (std::size_t i = 0; i < functions.size(); i++) {
      if (functions[i].name == name) {
        return i;
      }
    }
    throw FunctionNotFoundException{name};
  }
};

inline void operator<<(std::ostream& out, const Module& m) {
  int32_t index = 0;
  for (auto function : m.functions) {
    out << function;
    ++index;
  }
  for (auto string : m.strings) {
    out << "(string \"" << string << "\")" << std::endl;
  }
  out << std::endl;
}

inline bool operator==(const Module& lhs, const Module& rhs) {
  return lhs.functions == rhs.functions && lhs.strings == rhs.strings;
}

}  // namespace b9

#endif  // B9_MODULE_HPP_
