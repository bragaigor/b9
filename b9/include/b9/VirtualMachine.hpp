#ifndef B9_VIRTUALMACHINE_HPP_
#define B9_VIRTUALMACHINE_HPP_

#include <b9/Module.hpp>
#include <b9/OperandStack.hpp>
#include <b9/compiler/Compiler.hpp>
#include <b9/instructions.hpp>

#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/ObjectOperations.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Runtime.hpp>
#include <OMR/Om/ShapeOperations.hpp>
#include <OMR/Om/Value.hpp>

#include <cstring>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

extern "C" {
b9::PrimitiveFunction b9_prim_print_string;
b9::PrimitiveFunction b9_prim_print_number;
b9::PrimitiveFunction b9_prim_print_stack;
}

namespace b9 {

namespace Om = ::OMR::Om;

class Compiler;
class ExecutionContext;
class VirtualMachine;

struct Config {
  std::size_t maxInlineDepth = 0;  //< The JIT's max inline depth
  bool jit = false;                //< Enable the JIT
  bool directCall = false;         //< Enable direct JIT to JIT calls
  bool passParam = false;          //< Pass arguments in CPU registers
  bool lazyVmState = false;        //< Simulate the VM state
  bool debug = false;              //< Enable debug code
  bool verbose = false;            //< Enable verbose printing and tracing
};

inline std::ostream &operator<<(std::ostream &out, const Config &cfg) {
  out << std::boolalpha;
  out << "Mode:         " << (cfg.jit ? "JIT" : "Interpreter") << std::endl
      << "Inline depth: " << cfg.maxInlineDepth << std::endl
      << "directcall:   " << cfg.directCall << std::endl
      << "passparam:    " << cfg.passParam << std::endl
      << "lazyvmstate:  " << cfg.lazyVmState << std::endl
      << "debug:        " << cfg.debug;
  out << std::noboolalpha;
  return out;
}

struct BadFunctionCallException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

extern "C" typedef Om::RawValue (*JitFunction)(void *executionContext, ...);

class VirtualMachine {
 public:
  VirtualMachine(Om::ProcessRuntime &runtime, const Config &cfg);

  ~VirtualMachine() noexcept;

  /// Load a module into the VM.
  void load(std::shared_ptr<const Module> module, std::shared_ptr<ModuleMmap> moduleMmap);

  StackElement run(const std::size_t index,
                   const std::vector<StackElement> &usrArgs);

  StackElement run(const std::string &name,
                   const std::vector<StackElement> &usrArgs);

  const FunctionDef *getFunction(std::size_t index); // TODO: Remove this eventually

  void *getFunction(std::size_t index, bool dummy);

  std::uint32_t getNextInt32();

  std::uint32_t *getCurrentInstruction(void *funcPtr, std::uint32_t offset);

  PrimitiveFunction *getPrimitive(std::size_t index);

  JitFunction getJitAddress(std::size_t functionIndex);

  void setJitAddress(std::size_t functionIndex, JitFunction value);

  std::size_t getFunctionCount();

  std::string getFunctionName(void *funcPtr);

  std::uint32_t getFunctionNparams(void *funcPtr, std::uint32_t offset);

  std::uint32_t getFunctionNLocals(void *funcPtr, std::uint32_t offset);

  JitFunction generateCode(const std::size_t functionIndex);

  void generateAllCode();

  const char *getString(int index);

  const std::shared_ptr<const Module> &module() { return module_; }

  Om::MemorySystem &memoryManager() { return memoryManager_; }

  const Om::MemorySystem &memoryManager() const { return memoryManager_; }

  std::shared_ptr<Compiler> compiler() { return compiler_; }

  const Config &config() { return cfg_; }

 private:
  static constexpr PrimitiveFunction *const primitives_[] = {
      b9_prim_print_string, b9_prim_print_number, b9_prim_print_stack};

  Config cfg_;
  Om::MemorySystem memoryManager_;
  std::shared_ptr<Compiler> compiler_;
  std::shared_ptr<ModuleMmap> moduleMmap_; // TODO: Use this instead 
  std::shared_ptr<const Module> module_; // TODO: WE shouldn't need this with mmap...
  std::vector<JitFunction> compiledFunctions_; // TODO: Change to HashMap key: func name, value: JitFunction
};

}  // namespace b9

// define C callable Interpret API for each arg call
// if args are passed to the function, they are not passed
// on the intepreter stack

extern "C" {

using namespace b9;

Om::RawValue interpret(ExecutionContext *context,
                       const std::size_t functionIndex);

void primitive_call(ExecutionContext *context, Immediate value);
}

#endif  // B9_VIRTUALMACHINE_HPP_
