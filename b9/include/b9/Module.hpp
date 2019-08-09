#if !defined(B9_MODULE_HPP_)
#define B9_MODULE_HPP_

#include <b9/instructions.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream> 

namespace b9 {

class Compiler;
class ExecutionContext;
class VirtualMachine;

class FdHandle {
public:
    constexpr FdHandle() : value_(-1) {}

	FdHandle(const FdHandle&) = delete;

    FdHandle(FdHandle&& other) : value_(other.release()) {}

	constexpr explicit FdHandle(int value) : value_(value) {}

    ~FdHandle() {
        if (value_ != -1) {
            close(value_);
        }
    }

    /// Convert this handle to it's underlying fd.
    /// Happens implicitly (magically).
    /// This allows you to use an FdHandle anywhere you can use an int.
    constexpr operator int() const { return value_; }

    /// Move ownership of a fd from another handle to this handle.
    /// If this handle has a valid fd, close it.
    FdHandle& operator=(FdHandle&& other) {
        reset(other.release());
        return *this;
    }

    /// Get the underlying fd.
    ///
    constexpr int value() const { return value_; }

    /// clear this FdHandle, closing the underlying fd.
    ///
    void reset() {
        reset(-1);
    }

    /// Aquire ownership of a new fd, and close the old one.
    ///
    void reset(int fd) {
        if (value_ != -1) {
            close(value_);
        }
        value_ = fd;
    }

    /// Give up ownership of the fd, without closing it.
    /// @returns the underlying fd.
    int release() {
        int v = value_;
        value_ = -1;
        return v;
    }

private:
	int value_;
};

/// Move-only owner of an mmap region.
/// When the object goes out of scope, the mmapped region will be freed.
/// Note that, when a file is memory-mapped, it is safe to close the original fd, 
/// while continuing to use the mapping.
class MMapHandle {
public:
    MMapHandle(MMapHandle&& other)
        : addr_(other.addr_),
          size_(other.size_) {
        other.addr_ = nullptr;
        other.size_ = 0;
    }

    MMapHandle(void* addr, std::size_t size)
        : addr_(addr),
          size_(size) {}

    ~MMapHandle() {
        if (addr_ != nullptr) {
            munmap(addr_, size_);
        }
    }

    void* addr() const { return addr_; }
  
    std::size_t size() const { return size_; }

    void reset() {
        reset(nullptr, 0);
    }

    void reset(void* addr, std::size_t size) {
        if (addr_ != nullptr) {
            munmap(addr_, size_);
            addr_ = addr;
            size_ = size;
        }
    }

    MMapHandle& operator=(MMapHandle&& other) {
        reset(other.addr(), other.size());
        other.release();
        return *this;
    }

    void release() {
        addr_ = nullptr;
        size_ = 0;
    }

private:
    void* addr_;
    std::size_t size_;
};

inline MMapHandle map_file(const char* filename) {
  	FdHandle fd(open(filename, O_RDONLY));
  	if (fd == -1) {
      throw std::runtime_error(std::string("failed to open module: ") + filename);
    }

  	struct stat s;
  	int error = fstat(fd, &s);
	if (error != 0) {
      throw std::runtime_error(std::string("failed to stat module: ") + filename);
    }
    auto size = std::size_t(s.st_size);

    void* addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
      throw std::runtime_error(std::string("failed to mmap module: ") + filename);
    }

  std::cout << "fd: " << fd << ", size: " << size << ", addr: " << addr << std::endl;

	return { addr, size };
}

class Module2 {
public:
    Module2(MMapHandle&& data) : data_(std::move(data)) {

      void* addr = data_.addr();

      char* strChar = (char*)addr;
      for (size_t i = 0; i < 8; i++) {
        std::cout << strChar[i];
      }

      std::cout << std::endl;

      addr = static_cast<char*>(addr) + 8; // Skip header

      std::uint32_t funcSection = *((std::uint32_t*)(addr));
      if (funcSection != 1) {
        std::stringstream ss;
        ss << "Failed when trying to read function section. Found " << funcSection 
            << " instead. ";
        std::string message = ss.str();
        throw std::runtime_error(message);
      }
      addr = static_cast<std::uint32_t*>(addr) + 1;

      std::uint32_t functionCount = *((std::uint32_t*)(addr));
      addr = static_cast<std::uint32_t*>(addr) + 1;

      std::cout << "Found " << functionCount << " functions in program.\n";

      addrs_.reserve(functionCount);

      if (functionCount > 0) {
        for (size_t i = 0; i < functionCount; i++) {
          addrs_.push_back(addr);
          std::uint32_t funcNameLength = *((std::uint32_t*)(addr));
          addr = static_cast<std::uint32_t*>(addr) + 1;
          //******************************************************************************************
          std::cout << "Function len: " << funcNameLength << ", name: ";
          char* strChar2 = (char*)addr;
          for (size_t j = 0; j < funcNameLength; j++) {
            std::cout << strChar2[j];
          }
          std::cout << std::endl;
          //******************************************************************************************
          addr = static_cast<char*>(addr) + funcNameLength;
          addr = static_cast<std::uint32_t*>(addr) + 2; // Jump nparams and nlocals

          std::uint32_t *curInst = ((std::uint32_t *)addr);
          const Instruction *instructionPointer = (const Instruction *)curInst;
          int count = 0;
          while (*instructionPointer != END_SECTION) {
            instructionPointer++;
            addr = static_cast<std::uint32_t*>(addr) + 1;
            count++;
          }
          addr = static_cast<std::uint32_t*>(addr) + 1;
          std::cout << "There were " << count << " instructions in this last function.\n";
        }
      }

      std::uint32_t strSection = *((std::uint32_t*)(addr));
      if (strSection != 2) {
        std::stringstream ss;
        ss << "Failed when trying to read string section. Found code " << strSection 
            << " instead. ";
        std::string message = ss.str();
        throw std::runtime_error(message);
      }

      addr = static_cast<std::uint32_t*>(addr) + 1;
      std::uint32_t strCount = *((std::uint32_t*)(addr));
      addr = static_cast<std::uint32_t*>(addr) + 1;

      std::cout << "About to read string section. Found " << strCount << " strings.\n";

      if (strCount > 0) {
        for (size_t i = 0; i < strCount; i++) {
          std::string toRead;
          std::uint32_t strLen = *((std::uint32_t*)(addr));
          addr = static_cast<std::uint32_t*>(addr) + 1;
          char* strChar = (char*)addr;
          for (size_t i = 0; i < strLen; i++) {
            toRead.push_back(strChar[i]);
          }
          strings_.push_back(toRead);
          addr = static_cast<char*>(addr) + strLen;
        }
      }
    }

    void* getFunction(int index) const {
      return addrs_[index];
    }

    size_t getNumberOfFunctions() const {
      return addrs_.size();
    }

    size_t getNumberOfStrings() const {
      return strings_.size();
    }

    std::string getString(int index) const {
      return strings_[index];
    }

    static bool isSameFunction(void* funcAddr1, void* funcAddr2) {
      // TODO: How to compare 2 functions
      // Check function name
      // Check nparams and nlocals
      // Check instructions

      std::uint32_t funcNameLength1 = *((std::uint32_t*)(funcAddr1));
      funcAddr1 = static_cast<std::uint32_t*>(funcAddr1) + 1;
      std::uint32_t funcNameLength2 = *((std::uint32_t*)(funcAddr2));
      funcAddr2 = static_cast<std::uint32_t*>(funcAddr2) + 1;

      if (funcNameLength1 != funcNameLength2) {
        return false;
      }

      char* strChar1 = (char*)funcAddr1;
      char* strChar2 = (char*)funcAddr2;
      for (size_t j = 0; j < funcNameLength1; j++) {
        if (strChar1[j] != strChar2[j])
          return false;
      }

      funcAddr1 = static_cast<char*>(funcAddr1) + funcNameLength1;
      std::uint32_t nparams1 = *((std::uint32_t*)(funcAddr1));
      funcAddr1 = static_cast<std::uint32_t*>(funcAddr1) + 1; 
      std::uint32_t nlocals1 = *((std::uint32_t*)(funcAddr1));
      funcAddr1 = static_cast<std::uint32_t*>(funcAddr1) + 1;

      funcAddr2 = static_cast<char*>(funcAddr2) + funcNameLength2;
      std::uint32_t nparams2 = *((std::uint32_t*)(funcAddr2));
      funcAddr2 = static_cast<std::uint32_t*>(funcAddr2) + 1; 
      std::uint32_t nlocals2 = *((std::uint32_t*)(funcAddr2));
      funcAddr2 = static_cast<std::uint32_t*>(funcAddr2) + 1;

      if (nparams1 != nparams2 || nlocals1 != nlocals2) {
        return false;
      }

      std::uint32_t *curInst1 = ((std::uint32_t *)funcAddr1);
      const Instruction *instructionPointer1 = (const Instruction *)curInst1;
      std::uint32_t *curInst2 = ((std::uint32_t *)funcAddr2);
      const Instruction *instructionPointer2 = (const Instruction *)curInst2;

      while (*instructionPointer1 != END_SECTION) {
        if (instructionPointer1->raw() != instructionPointer2->raw())
          return false;
        instructionPointer1++;
        instructionPointer2++;
      }
      
      return true;
    }

private:
    MMapHandle data_;
    std::vector<void*> addrs_;
    std::vector<std::string> strings_;
};

// Function Definition

struct FunctionDef {
  std::string name;
  std::vector<Instruction> instructions;
  std::uint32_t nparams;
  std::uint32_t nlocals;
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

inline bool operator==(const Module2& lhs, const Module2& rhs) {

  if (&lhs == &rhs) {
    std::cout << "lhs address: " << &lhs << ", rhs address: " << &rhs << std::endl;
    return true;
  } 

  if (lhs.getNumberOfFunctions() != rhs.getNumberOfFunctions())
    return false;

  for (size_t i = 0; i < lhs.getNumberOfFunctions(); i++) {

    void *currFunc1 = lhs.getFunction(i);
    void *currFunc2 = rhs.getFunction(i);

    if (!Module2::isSameFunction(currFunc1, currFunc2))
      return false;
  }

  // Make sure string sections match each other
  if (lhs.getNumberOfStrings() != rhs.getNumberOfStrings())
    return false;

  if (lhs.getNumberOfStrings() > 0) {

    for (size_t i = 0; i < lhs.getNumberOfStrings(); i++) {
      std::string currStr1 = lhs.getString(i);
      std::string currStr2 = rhs.getString(i);
      if ( currStr1 != currStr2)
        return false;
    }
  }

  return true;
}


}  // namespace b9

#endif  // B9_MODULE_HPP_
