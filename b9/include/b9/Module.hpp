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
  // TODO: Do we need this?
  // std::shared_ptr<ModuleMmap> getInstance(std::uint32_t size) {
  //   if(_moduleMmap == NULL) {
  //     _moduleMmap = std::make_shared<ModuleMmap>(size);
  //   }

  //   return _moduleMmap;
  // }

  ~ModuleMmap() {
    if (_currentPtr != NULL) {
      munmap(_currentPtr, _size);
    }
  }

  ModuleMmap(std::uint32_t size) : _size(size), _index(0) {

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
      throw ModuleException{"Error when trying to mmap Module"};
      
    } else {
      _currentPtr = addr;
      std::cout << "mmap() successfully.\n";
    }


  }

  void *getFunction(std::size_t index) {
    std::string funcName;
    if (_indexToStr.find(index) == _indexToStr.end()) {
		std::stringstream ss;
		ss << " Error trying to find index " << index << " in map.";
		std::string message = ss.str();
		throw ModuleException{message};
    } else {
		funcName = _indexToStr[index];
    }

    return getFunction(funcName);
  }

  std::uint32_t *getCurrentInstruction() {
	  std::uint32_t *toReturn = ((std::uint32_t *)_currentPtr);
	  _currentPtr = static_cast<char*>(_currentPtr) + INSTRUCTION_SIZE;
	  return toReturn;
  }

void *getFunction(std::string funcName) {
	  void* function = NULL;

    if (_stringToAddr.find(funcName) == _stringToAddr.end()) {
	// for ( auto it = _stringToAddr.begin(); it != _stringToAddr.end(); ++it )
    // 	std::cout << "\t\t #### " << it->first << ":" << it->second << std::endl;

      std::stringstream ss;
      ss << "Error trying to find function with name: " << funcName << " in map.";
      std::string message = ss.str();
      throw ModuleException{message};
    } else {
		  function = _stringToAddr[funcName];
      _currentPtr = function;
    }

    return function;
  }

  bool insertNewFunc(std::string &functionName) {
    std::cout << "Inside insertNewFunc(): " << _index << ", with val: " << functionName << ", at address: " << _currentPtr << std::endl;
    _indexToStr[_index++] = functionName;
    _stringToAddr[functionName] = _currentPtr;

    // TODO: Probably do not need this because there's already a map that contains the strings 
    // std::uint32_t funcLength = functionName.length();
    // *static_cast<std::uint32_t*>(_currentPtr) = funcLength;
    // _currentPtr = static_cast<char*>(_currentPtr) + sizeof(std::uint32_t);
    // std::memcpy(_currentPtr, functionName.c_str(), funcLength); 
    // _currentPtr = static_cast<char*>(_currentPtr) + funcLength; 

    return true;
  }

  bool insertString(std::string str) {
    std::uint32_t funcLength = str.length();
    *static_cast<std::uint32_t*>(_currentPtr) = funcLength; 
    _currentPtr = static_cast<char*>(_currentPtr) + INSTRUCTION_SIZE;

    std::memcpy(_currentPtr, str.c_str(), funcLength);

    _currentPtr = static_cast<char*>(_currentPtr) + funcLength;

    return true;
  }

  bool insertParams(std::uint32_t nparams, std::uint32_t nlocals) {
    *static_cast<std::uint32_t*>(_currentPtr) = nparams;
    _currentPtr = static_cast<char*>(_currentPtr) + INSTRUCTION_SIZE;
    *static_cast<std::uint32_t*>(_currentPtr) = nlocals;
    _currentPtr = static_cast<char*>(_currentPtr) + INSTRUCTION_SIZE;

    return true;
  }

  bool insertInstruction(std::uint32_t instruction) {
	//   std::uint32_t instruc = *((std::uint32_t*)(instruction));
	std::cout << "Inserting instruction 0x" << std::hex << instruction << std::dec << ", at address: " << _currentPtr << std::endl;

    *static_cast<std::uint32_t*>(_currentPtr) = instruction;
    _currentPtr = static_cast<char*>(_currentPtr) + INSTRUCTION_SIZE;

    return true;
  }

  std::uint32_t getNumberOfFunctions() {
    return _index;
  }

  std::uint32_t getNextInt32Val() {

    std::uint32_t myNumber = *((std::uint32_t*)(_currentPtr));
    std::cout << "Inside getNextInt32Val(), returning num: 0x" << std::hex << myNumber << std::dec << ", at address: " << _currentPtr << std::endl;

    _currentPtr = static_cast<char*>(_currentPtr) + INSTRUCTION_SIZE;

    return myNumber;
  }

  private:

  std::unordered_map<std::string, void*> _stringToAddr; // We do need it because of b9test
  std::unordered_map<std::uint32_t, std::string> _indexToStr; // Maps indexes to function names TODO: Try to get rid of it
  void* _currentPtr;
  std::uint32_t _size;
  // std::shared_ptr<ModuleMmap> _moduleMmap;
  std::uint32_t _index;
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
