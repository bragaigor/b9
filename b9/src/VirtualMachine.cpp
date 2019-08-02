#include <b9/ExecutionContext.hpp>
#include <b9/VirtualMachine.hpp>
#include <b9/compiler/Compiler.hpp>

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/ArrayOperations.hpp>
#include <OMR/Om/ObjectOperations.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/ShapeOperations.hpp>
#include <OMR/Om/Value.hpp>

#include <omrgc.h>
#include <Jit.hpp>

#include <sys/time.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace b9 {

constexpr PrimitiveFunction *const VirtualMachine::primitives_[3];

VirtualMachine::VirtualMachine(Om::ProcessRuntime &runtime, const Config &cfg)
    : cfg_{cfg}, memoryManager_(runtime), compiler_{nullptr} {
  if (cfg_.verbose) std::cout << "VM initializing..." << std::endl;

  if (cfg_.jit) {
    auto ok = initializeJit();
    if (!ok) {
      throw std::runtime_error{"Failed to init JIT"};
    }

    compiler_ = std::make_shared<Compiler>(*this, cfg_);
  }
}

VirtualMachine::~VirtualMachine() noexcept {
  if (cfg_.jit) {
    shutdownJit();
  }
}

void VirtualMachine::load(std::shared_ptr<const Module> module, std::shared_ptr<ModuleMmap> moduleMmap) {
  module_ = module;
  moduleMmap_ = moduleMmap;
  compiledFunctions_.reserve(getFunctionCount());
}

/// OpCode Interpreter

JitFunction VirtualMachine::getJitAddress(std::size_t functionIndex) {
  if (functionIndex >= compiledFunctions_.size()) {
    return nullptr;
  }
  return compiledFunctions_[functionIndex];
}

void VirtualMachine::setJitAddress(std::size_t functionIndex,
                                   JitFunction value) {
  compiledFunctions_[functionIndex] = value;
}

PrimitiveFunction *VirtualMachine::getPrimitive(std::size_t index) {
  return primitives_[index];
}

const FunctionDef *VirtualMachine::getFunction(std::size_t index) {
  return &module_->functions[index];
}

void *VirtualMachine::getFunction(std::size_t index, bool dummy) {
  return moduleMmap_->getFunction(index);
}

// void *VirtualMachine::getFunction(const std::string name) {
//   return moduleMmap_->getFunction(name);
// }

JitFunction VirtualMachine::generateCode(const std::size_t functionIndex) {
  try {
    return compiler_->generateCode(functionIndex);
  } catch (const CompilationException &e) {
    auto f = getFunction(functionIndex);
    std::cerr << "Warning: Failed to compile " << f << std::endl;
    std::cerr << "    with error: " << e.what() << std::endl;
    return nullptr;
  }
}

// TODO: Change this to point to the string section of the new Module!!!! 
const char *VirtualMachine::getString(int index) {
  std::cout << "Inside VirtualMachine::getString()\n";
  std::cout << "Returning string: " << moduleMmap_->strings[index] << "\n";
  std::cout << " ??????????????????? Returning string: " << moduleMmap_->getString(index) << "\n";
  return moduleMmap_->getString(index);
  // return moduleMmap_->strings[index];
  // return module_->strings[index];
}

std::size_t VirtualMachine::getFunctionCount() {
  // return moduleMmap_->getNumberOfFunctions();
  return module_->functions.size();
}

void VirtualMachine::generateAllCode() {
  assert(cfg_.jit);
  auto functionIndex = 0;  // 0 index for <script>

  while (functionIndex < getFunctionCount()) {
    void *funcPtr = getFunction(functionIndex, true);
    if (cfg_.debug) {
      std::string funcName = getFunctionName(funcPtr);
      std::cout << "\nJitting function: " << funcName
                << " of index: " << functionIndex << std::endl;
    }
    auto func = compiler_->generateCode(functionIndex);
    compiledFunctions_.push_back(func);
    ++functionIndex;
  }
}

std::uint32_t VirtualMachine::getNextInt32() {
  return moduleMmap_->getNextInt32Val();
}

std::uint32_t *VirtualMachine::getCurrentInstruction(void *funcPtr, std::uint32_t offset) {
  return moduleMmap_->getCurrentInstruction(funcPtr, offset);
}

StackElement VirtualMachine::run(const std::string &name,
                                 const std::vector<StackElement> &usrArgs) {

  return run(module_->getFunctionIndex(name), usrArgs);
}

std::string VirtualMachine::getFunctionName(void *funcPtr) {
  return moduleMmap_->getFunctionName(funcPtr);
}

std::uint32_t VirtualMachine::getFunctionNparams(void *funcPtr, std::uint32_t offset) {
  return moduleMmap_->getFunctionNparams(funcPtr, offset);
}

std::uint32_t VirtualMachine::getFunctionNLocals(void *funcPtr, std::uint32_t offset) {
  return moduleMmap_->getFunctionNLocals(funcPtr, offset);
}

StackElement VirtualMachine::run(const std::size_t functionIndex,
                                 const std::vector<StackElement> &usrArgs) {
  // auto function = getFunction(functionIndex);
  void *funcPtr = getFunction(functionIndex, true);

  std::string funcName = getFunctionName(funcPtr);
  std::uint32_t nparams = getFunctionNparams(funcPtr, funcName.size() + INSTRUCTION_SIZE);

  auto paramsCount = nparams;

  ExecutionContext *executionContext = new ExecutionContext(*this, cfg_);

  if (cfg_.verbose) {
    std::cout << "+++++++++++++++++++++++" << std::endl;
    std::cout << "Running function: " << funcName
              << " nparams: " << nparams << std::endl;
  }

  if (nparams != usrArgs.size()) {
    std::stringstream ss;
    ss << funcName << " - Got " << usrArgs.size()
       << " arguments, expected " << nparams;
    std::string message = ss.str();
    throw BadFunctionCallException{message};
  }

  // std::cout << "Their n params: " << function->nparams << ", my nparams: " << nparams << std::endl;
  // push user defined arguments to send to the program
  for (std::size_t i = 0; i < nparams; i++) {
    auto idx = nparams - i - 1;
    auto arg = usrArgs[idx];
    executionContext->push(arg);
  }

  StackElement result = executionContext->interpret(functionIndex); // TODO: Change to function name

  return result;
}

}  // namespace b9

//
// Jit to Interpreter transitions
//

extern "C" {

Om::RawValue interpret(ExecutionContext *context,
                       const std::size_t functionIndex) {
  return (Om::RawValue)context->interpret(functionIndex);
}

// For primitive calls
void primitive_call(ExecutionContext *context, Immediate value) {
  context->doPrimitiveCall(value);
}

}  // extern "C"
