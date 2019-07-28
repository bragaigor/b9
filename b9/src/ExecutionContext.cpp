#include <b9/ExecutionContext.hpp>
#include <b9/VirtualMachine.hpp>
#include <b9/compiler/Compiler.hpp>

#include <omrgc.h>
#include "Jit.hpp"

#include <OMR/Om/ArrayOperations.hpp>
#include <OMR/Om/ObjectOperations.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/ShapeOperations.hpp>
#include <OMR/Om/Value.hpp>

#include <sys/time.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace b9 {

ExecutionContext::ExecutionContext(VirtualMachine &virtualMachine,
                                   const Config &cfg)
    : omContext_(virtualMachine.memoryManager()),
      virtualMachine_(&virtualMachine),
      cfg_(&cfg) {
  omContext().userMarkingFns().push_back(
      [this](Om::MarkingVisitor &v) { this->visit(v); });
}

void ExecutionContext::reset() {
  stack_.reset();
  programCounter_ = 0;
}

Om::Value ExecutionContext::callJitFunction(JitFunction jitFunction,
                                            std::size_t nparams) {
  Om::RawValue result = 0;

  if (cfg_->passParam) {
    if (cfg_->verbose) {
      std::cout << "Int: transition to Jit(PP): " << (void *)jitFunction
                << std::endl;
    }
    switch (nparams) {
      case 0: {
        result = jitFunction(this);
      } break;
      case 1: {
        StackElement p1 = pop();
        result = jitFunction(this, p1.raw());
      } break;
      case 2: {
        StackElement p2 = pop();
        StackElement p1 = pop();
        result = jitFunction(this, p1.raw(), p2.raw());
      } break;
      case 3: {
        StackElement p3 = pop();
        StackElement p2 = pop();
        StackElement p1 = pop();
        result = (*jitFunction)(this, p1.raw(), p2.raw(), p3.raw());
      } break;
      default:
        throw std::runtime_error{"Need to add handlers for more parameters"};
        break;
    }
  } else {
    if (cfg_->verbose) {
      std::cout << "Int: transition to Jit: " << (void *)jitFunction
                << std::endl;
    }
    result = jitFunction(this);
  }

  return Om::Value(Om::AS_RAW, result);
}

StackElement ExecutionContext::interpret(const std::size_t functionIndex) { // TODO: Change to function name
  auto function = virtualMachine_->getFunction(functionIndex);
  auto paramsCount = function->nparams;
  auto localsCount = function->nlocals;
  auto jitFunction = virtualMachine_->getJitAddress(functionIndex);

  if (cfg_->debug) {
    std::cerr << "intepret: " << function->name
              << " nparams: " << function->nparams << std::endl;
  }

  if (jitFunction) {
    return callJitFunction(jitFunction, paramsCount);
  }

  // interpret the method otherwise
  const Instruction *instructionPointer = function->instructions.data();

  StackElement *params = stack_.top() - paramsCount;

  stack_.pushn(localsCount);  // make room for locals in the stack
  StackElement *locals = stack_.top() - localsCount;

  while (*instructionPointer != END_SECTION) {
    switch (instructionPointer->opCode()) {
      case OpCode::FUNCTION_CALL:
        doFunctionCall(instructionPointer->immediate());
        break;
      case OpCode::FUNCTION_RETURN: {
        auto result = stack_.pop();
        stack_.restore(params);
        return result;
        break;
      }
      case OpCode::PRIMITIVE_CALL:
        doPrimitiveCall(instructionPointer->immediate());
        break;
      case OpCode::JMP:
        instructionPointer += instructionPointer->immediate();
        break;
      case OpCode::DUPLICATE:
        doDuplicate();
        break;
      case OpCode::DROP:
        doDrop();
        break;
      case OpCode::PUSH_FROM_LOCAL:
        doPushFromLocal(locals, instructionPointer->immediate());
        break;
      case OpCode::POP_INTO_LOCAL:
        doPopIntoLocal(locals, instructionPointer->immediate());
        break;
      case OpCode::PUSH_FROM_PARAM:
        doPushFromParam(params, instructionPointer->immediate());
        break;
      case OpCode::POP_INTO_PARAM:
        doPopIntoParam(params, instructionPointer->immediate());
        break;
      case OpCode::INT_ADD:
        doIntAdd();
        break;
      case OpCode::INT_SUB:
        doIntSub();
        break;
      case OpCode::INT_MUL:
        doIntMul();
        break;
      case OpCode::INT_DIV:
        doIntDiv();
        break;
      case OpCode::INT_PUSH_CONSTANT:
        doIntPushConstant(instructionPointer->immediate());
        break;
      case OpCode::INT_NOT:
        doIntNot();
        break;
      case OpCode::JMP_EQ:
        instructionPointer += doJmpEq(instructionPointer->immediate());
        break;
      case OpCode::JMP_NEQ:
        instructionPointer += doJmpNeq(instructionPointer->immediate());
        break;
      case OpCode::JMP_GT:
        instructionPointer += doJmpGt(instructionPointer->immediate());
        break;
      case OpCode::JMP_GE:
        instructionPointer += doJmpGe(instructionPointer->immediate());
        break;
      case OpCode::JMP_LT:
        instructionPointer += doJmpLt(instructionPointer->immediate());
        break;
      case OpCode::JMP_LE:
        instructionPointer += doJmpLe(instructionPointer->immediate());
        break;
      case OpCode::STR_PUSH_CONSTANT:
        doStrPushConstant(instructionPointer->immediate());
        break;
      case OpCode::NEW_OBJECT:
        doNewObject();
        break;
      case OpCode::PUSH_FROM_OBJECT:
        doPushFromObject(Om::Id(instructionPointer->immediate()));
        break;
      case OpCode::POP_INTO_OBJECT:
        doPopIntoObject(Om::Id(instructionPointer->immediate()));
        break;
      case OpCode::CALL_INDIRECT:
        doCallIndirect();
        break;
      case OpCode::SYSTEM_COLLECT:
        doSystemCollect();
        break;
      default:
        assert(false);
        break;
    }
    instructionPointer++;
    programCounter_++;
  }
  throw std::runtime_error("Reached end of function");
}

StackElement ExecutionContext::interpret(std::string functionName) {
  void *function = virtualMachine_->getFunction(functionName);
  std::uint32_t paramsCount = virtualMachine_->getNextInt32();
  std::uint32_t localsCount = virtualMachine_->getNextInt32();
  // auto jitFunction = virtualMachine_->getJitAddress(functionIndex); // TODO: Fix this!!!!!!

  std::cout << "Read functionName: " << functionName << " nparams: " 
          << paramsCount << " localsCount: " << localsCount << std::endl;

  if (cfg_->debug) {
    std::cerr << "intepret: " << functionName
              << " nparams: " << paramsCount << std::endl;
  }

  // if (jitFunction) {
  //   return callJitFunction(jitFunction, paramsCount);
  // }

  // interpret the method otherwise
  std::uint32_t *thisInstruction = virtualMachine_->getCurrentInstruction();
  const Instruction *instructionPointer = (const Instruction *)thisInstruction;

  StackElement *params = stack_.top() - paramsCount;

  stack_.pushn(localsCount);  // make room for locals in the stack
  StackElement *locals = stack_.top() - localsCount;

  std::cout << "Current raw instruction is: " << std::hex << *thisInstruction << std::dec << std::endl;

  while (*instructionPointer != END_SECTION) {
    switch (instructionPointer->opCode()) {
      case OpCode::FUNCTION_CALL:
        std::cout << "Calling OpCode::FUNCTION_CALL\n";
        doFunctionCall(instructionPointer->immediate());
        break;
      case OpCode::FUNCTION_RETURN: {
        std::cout << "Calling OpCode::FUNCTION_RETURN\n";
        auto result = stack_.pop();
        stack_.restore(params);
        return result;
        break;
      }
      case OpCode::PRIMITIVE_CALL:
        std::cout << "Calling OpCode::PRIMITIVE_CALL\n";
        doPrimitiveCall(instructionPointer->immediate());
        break;
      case OpCode::JMP:
        std::cout << "Calling OpCode::JMP\n";
        instructionPointer += instructionPointer->immediate();
        break;
      case OpCode::DUPLICATE:
        std::cout << "Calling OpCode::DUPLICATE\n";
        doDuplicate();
        break;
      case OpCode::DROP:
        std::cout << "Calling OpCode::DROP\n";
        doDrop();
        break;
      case OpCode::PUSH_FROM_LOCAL:
        std::cout << "Calling OpCode::PUSH_FROM_LOCAL\n";
        doPushFromLocal(locals, instructionPointer->immediate());
        break;
      case OpCode::POP_INTO_LOCAL:
        std::cout << "Calling OpCode::POP_INTO_LOCAL\n";
        doPopIntoLocal(locals, instructionPointer->immediate());
        break;
      case OpCode::PUSH_FROM_PARAM:
        std::cout << "Calling OpCode::PUSH_FROM_PARAM\n";
        doPushFromParam(params, instructionPointer->immediate());
        break;
      case OpCode::POP_INTO_PARAM:
        std::cout << "Calling OpCode::POP_INTO_PARAM\n";
        doPopIntoParam(params, instructionPointer->immediate());
        break;
      case OpCode::INT_ADD:
        std::cout << "Calling OpCode::INT_ADD\n";
        doIntAdd();
        break;
      case OpCode::INT_SUB:
        std::cout << "Calling OpCode::INT_SUB\n";
        doIntSub();
        break;
      case OpCode::INT_MUL:
        std::cout << "Calling OpCode::INT_MUL\n";
        doIntMul();
        break;
      case OpCode::INT_DIV:
        std::cout << "Calling OpCode::INT_DIV\n";
        doIntDiv();
        break;
      case OpCode::INT_PUSH_CONSTANT:
        std::cout << "Calling OpCode::INT_PUSH_CONSTANT\n";
        doIntPushConstant(instructionPointer->immediate());
        break;
      case OpCode::INT_NOT:
        std::cout << "Calling OpCode::INT_NOT\n";
        doIntNot();
        break;
      case OpCode::JMP_EQ:
        std::cout << "Calling OpCode::JMP_EQ\n";
        instructionPointer += doJmpEq(instructionPointer->immediate());
        break;
      case OpCode::JMP_NEQ:
        std::cout << "Calling OpCode::JMP_NEQ\n";
        instructionPointer += doJmpNeq(instructionPointer->immediate());
        break;
      case OpCode::JMP_GT:
        std::cout << "Calling OpCode::JMP_GT\n";
        instructionPointer += doJmpGt(instructionPointer->immediate());
        break;
      case OpCode::JMP_GE:
        std::cout << "Calling OpCode::JMP_GE\n";
        instructionPointer += doJmpGe(instructionPointer->immediate());
        break;
      case OpCode::JMP_LT:
        std::cout << "Calling OpCode::JMP_LT\n";
        instructionPointer += doJmpLt(instructionPointer->immediate());
        break;
      case OpCode::JMP_LE:
        std::cout << "Calling OpCode::JMP_LE\n";
        instructionPointer += doJmpLe(instructionPointer->immediate());
        break;
      case OpCode::STR_PUSH_CONSTANT:
        std::cout << "Calling OpCode::STR_PUSH_CONSTANT\n";
        doStrPushConstant(instructionPointer->immediate());
        break;
      case OpCode::NEW_OBJECT:
        std::cout << "Calling OpCode::NEW_OBJECT\n";
        doNewObject();
        break;
      case OpCode::PUSH_FROM_OBJECT:
        std::cout << "Calling OpCode::PUSH_FROM_OBJECT\n";
        doPushFromObject(Om::Id(instructionPointer->immediate()));
        break;
      case OpCode::POP_INTO_OBJECT:
        std::cout << "Calling OpCode::POP_INTO_OBJECT\n";
        doPopIntoObject(Om::Id(instructionPointer->immediate()));
        break;
      case OpCode::CALL_INDIRECT:
        std::cout << "Calling OpCode::CALL_INDIRECT\n";
        doCallIndirect();
        break;
      case OpCode::SYSTEM_COLLECT:
        std::cout << "Calling OpCode::SYSTEM_COLLECT\n";
        doSystemCollect();
        break;
      default:
        assert(false);
        break;
    }
    std::cout << "Updating instruction...\n";
    instructionPointer++;
    programCounter_++;
  }
  throw std::runtime_error("Reached end of function");
}

void ExecutionContext::push(StackElement value) { stack_.push(value); }

StackElement ExecutionContext::pop() { return stack_.pop(); }

void ExecutionContext::doFunctionCall(Immediate value) {
  // TODO: From the immediate get the function name which is already stored inside virtualMachine_
  //      in Module class, then we can get the function and call interpret 
  auto f = virtualMachine_->getFunction((std::size_t)value);
  auto result = interpret(value);
  push(result);
}

void ExecutionContext::doFunctionReturn(StackElement returnVal) {
  // TODO
}

void ExecutionContext::doPrimitiveCall(Immediate value) {
  PrimitiveFunction *primitive = virtualMachine_->getPrimitive(value);
  (*primitive)(this);
}

Immediate ExecutionContext::doJmp(Immediate offset) { return offset; }

void ExecutionContext::doDuplicate() { push(stack_.peek()); }

void ExecutionContext::doDrop() { stack_.pop(); }

void ExecutionContext::doPushFromLocal(StackElement *locals, Immediate offset) {
  stack_.push(locals[offset]);
}

void ExecutionContext::doPopIntoLocal(StackElement *locals, Immediate offset) {
  locals[offset] = stack_.pop();
}

void ExecutionContext::doPushFromParam(StackElement *params, Immediate offset) {
  stack_.push(params[offset]);
}

void ExecutionContext::doPopIntoParam(StackElement *params, Immediate offset) {
  params[offset] = stack_.pop();
}

void ExecutionContext::doIntAdd() {
  auto right = stack_.pop().getInt48();
  auto left = stack_.pop().getInt48();
  push({Om::AS_INT48, left + right});
}

void ExecutionContext::doIntSub() {
  auto right = stack_.pop().getInt48();
  auto left = stack_.pop().getInt48();
  push({Om::AS_INT48, left - right});
}

void ExecutionContext::doIntMul() {
  auto right = stack_.pop().getInt48();
  auto left = stack_.pop().getInt48();
  push({Om::AS_INT48, left * right});
}

void ExecutionContext::doIntDiv() {
  auto right = stack_.pop().getInt48();
  auto left = stack_.pop().getInt48();
  push({Om::AS_INT48, left / right});
}

void ExecutionContext::doIntPushConstant(Immediate value) {
  stack_.push({Om::AS_INT48, static_cast<std::int64_t>(value)});
}

void ExecutionContext::doIntNot() {
  auto x = stack_.pop();
  assert(x.isInt48());
  push({Om::AS_INT48, !(x.getInt48())});
}

Immediate ExecutionContext::doJmpEq(Immediate delta) {
  auto right = stack_.pop();
  auto left = stack_.pop();
  if (left == right) {
    return delta;
  }
  return 0;
}

Immediate ExecutionContext::doJmpNeq(Immediate delta) {
  auto right = stack_.pop();
  auto left = stack_.pop();
  if (left != right) {
    return delta;
  }
  return 0;
}

Immediate ExecutionContext::doJmpGt(Immediate delta) {
  auto right = stack_.pop();
  auto left = stack_.pop();

  if (right.isInt48() && left.isInt48()) {
    if (left.getInt48() > right.getInt48()) {
      return delta;
    }
  } else if (right.isUint48() && left.isUint48()) {
    const auto &strRight = virtualMachine_->getString(right.getUint48());
    const auto &strLeft = virtualMachine_->getString(left.getUint48());
    if (strLeft > strRight) {
      return delta;
    }
  } else {
    throw std::runtime_error("Operands for comparison not of same type.");
  }

  return 0;
}

// ( left right -- )
Immediate ExecutionContext::doJmpGe(Immediate delta) {
  auto right = stack_.pop();
  auto left = stack_.pop();

  if (right.isInt48() && left.isInt48()) {
    if (left.getInt48() >= right.getInt48()) {
      return delta;
    }
  } else if (right.isUint48() && left.isUint48()) {
    const auto &strRight = virtualMachine_->getString(right.getUint48());
    const auto &strLeft = virtualMachine_->getString(left.getUint48());
    if (strLeft >= strRight) {
      return delta;
    }
  } else {
    throw std::runtime_error("Operands for comparison not of same type.");
  }

  return 0;
}

// ( left right -- )
Immediate ExecutionContext::doJmpLt(Immediate delta) {
  auto right = stack_.pop();
  auto left = stack_.pop();

  if (right.isInt48() && left.isInt48()) {
    if (left.getInt48() < right.getInt48()) {
      return delta;
    }
  } else if (right.isUint48() && left.isUint48()) {
    const auto &strRight = virtualMachine_->getString(right.getUint48());
    const auto &strLeft = virtualMachine_->getString(left.getUint48());
    if (strLeft < strRight) {
      return delta;
    }
  } else {
    throw std::runtime_error("Operands for comparison not of same type.");
  }

  return 0;
}

// ( left right -- )
Immediate ExecutionContext::doJmpLe(Immediate delta) {
  auto right = stack_.pop();
  auto left = stack_.pop();

  if (right.isInt48() && left.isInt48()) {
    if (left.getInt48() <= right.getInt48()) {
      return delta;
    }
  } else if (right.isUint48() && left.isUint48()) {
    const auto &strRight = virtualMachine_->getString(right.getUint48());
    const auto &strLeft = virtualMachine_->getString(left.getUint48());
    if (strLeft <= strRight) {
      return delta;
    }
  } else {
    throw std::runtime_error("Operands for comparison not of same type.");
  }

  return 0;
}

// ( -- string )
void ExecutionContext::doStrPushConstant(Immediate param) {
  assert(param >= 0);
  stack_.push({Om::AS_UINT48, static_cast<std::uint64_t>(param)});
}

// ( -- object )
void ExecutionContext::doNewObject() {
  auto ref = Om::allocateEmptyObject(*this);
  stack_.push(Om::Value{Om::AS_REF, ref});
}

// ( object -- value )
void ExecutionContext::doPushFromObject(Om::Id slotId) {
  auto value = stack_.pop();
  if (!value.isRef()) {
    throw std::runtime_error("Accessing non-object value as an object.");
  }
  auto obj = value.getRef<Om::Object>();
  Om::SlotDescriptor descriptor;
  auto found = Om::lookupSlot(*this, obj, slotId, descriptor);
  if (found) {
    Om::Value result;
    result = Om::getValue(*this, obj, descriptor);
    stack_.push(result);
  } else {
    throw std::runtime_error("Accessing an object's field that doesn't exist.");
  }
}

// ( object value -- )
void ExecutionContext::doPopIntoObject(Om::Id slotId) {
  if (!stack_.peek().isRef()) {
    throw std::runtime_error("Accessing non-object as an object");
  }

  std::size_t offset = 0;
  auto object = stack_.pop().getRef<Om::Object>();

  Om::SlotDescriptor descriptor;
  bool found = Om::lookupSlot(*this, object, slotId, descriptor);

  if (!found) {
    static constexpr Om::SlotType type(Om::Id(0), Om::CoreType::VALUE);

    Om::RootRef<Om::Object> root(*this, object);
    auto map = Om::transitionLayout(*this, root, {{type, slotId}});
    assert(map != nullptr);

    // TODO: Get the descriptor fast after a single-slot transition.
    Om::lookupSlot(*this, object, slotId, descriptor);
    object = root.get();
  }

  auto val = pop();
  Om::setValue(*this, object, descriptor, val);
  // TODO: Write barrier the object on store.
}

void ExecutionContext::doCallIndirect() {
  assert(0);  // TODO: Implement call indirect
}

void ExecutionContext::doSystemCollect() {
  std::cout << "SYSTEM COLLECT!!!" << std::endl;
  OMR_GC_SystemCollect(omContext_.vmContext(), 0);
}

}  // namespace b9
