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

void VirtualMachine::load(std::shared_ptr<const Module> module) {
  module_ = module;
  compiledFunctions_.reserve(getFunctionCount());
}

void VirtualMachine::load2(const char* moduleName) {
  module2_ = load_module(moduleName);
  // module2_ = std::make_shared<Module2>(mmap_file(moduleName));
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

const FunctionDef *VirtualMachine::getFunction2(std::size_t index) {
  return &module2_->functions_[index];
}

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

const std::string &VirtualMachine::getString(int index) {
  return module_->strings[index];
}

// TODO: Make it use the new module
std::size_t VirtualMachine::getFunctionCount() {
  return module_->functions.size();
}

void VirtualMachine::generateAllCode() {
  assert(cfg_.jit);
  auto functionIndex = 0;  // 0 index for <script>

  while (functionIndex < getFunctionCount()) {
    if (cfg_.debug)
      std::cout << "\nJitting function: " << getFunction(functionIndex)->name
                << " of index: " << functionIndex << std::endl;
    auto func = compiler_->generateCode(functionIndex);
    compiledFunctions_.push_back(func);
    ++functionIndex;
  }
}

StackElement VirtualMachine::run(const std::string &name,
                                 const std::vector<StackElement> &usrArgs) {
  return run(module_->getFunctionIndex(name), usrArgs);
}

StackElement VirtualMachine::run(const std::size_t functionIndex,
                                 const std::vector<StackElement> &usrArgs) {
  auto function = getFunction(functionIndex);
  auto function2 = getFunction2(functionIndex);

  auto paramsCount = function->nparams;

  ExecutionContext *executionContext = new ExecutionContext(*this, cfg_);

  std::cout << "The following must match:\n";
  std::cout << function->name << " :: " << function2->name << " | " 
            << function->nparams << " :: " << function2->nparams << " | " 
            << function->nlocals << " :: " << function2->nlocals << std::endl;

  if (cfg_.verbose) {
    std::cout << "+++++++++++++++++++++++" << std::endl;
    std::cout << "Running function: " << function->name
              << " nparams: " << paramsCount << std::endl;
  }

  if (paramsCount != usrArgs.size()) {
    std::stringstream ss;
    ss << function->name << " - Got " << usrArgs.size()
       << " arguments, expected " << paramsCount;
    std::string message = ss.str();
    throw BadFunctionCallException{message};
  }

  // push user defined arguments to send to the program
  for (std::size_t i = 0; i < paramsCount; i++) {
    auto idx = paramsCount - i - 1;
    auto arg = usrArgs[idx];
    executionContext->push(arg);
  }

  StackElement result = executionContext->interpret(functionIndex);

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
