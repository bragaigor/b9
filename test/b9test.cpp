#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <b9/ExecutionContext.hpp>
#include <b9/deserialize.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>

#include <gtest/gtest.h>

namespace b9 {
namespace test {

using namespace OMR::Om;

// clang-format off
const std::vector<const char*> TEST_NAMES = {
  "test_return_true",
  "test_return_false",
  "test_add",
  "test_sub",
  "test_mul",
  "test_div",
  // "test_not",
  "test_equal",
  "test_equal_1",
  "test_greaterThan",
  "test_greaterThan_1",
  "test_greaterThanOrEqual",
  "test_greaterThanOrEqual_1",
  "test_greaterThanOrEqual_2",
  "test_lessThan",
  "test_lessThan_1",
  "test_lessThan_2",
  "test_lessThan_3",
  "test_lessThanOrEqual",
  "test_lessThanOrEqual_1",
  "test_call",
  "test_string_declare_string_var",
  "test_string_return_string",
  "test_while",
  "test_for_never_run_body",
  "test_for_sum"
};

const RawInstruction OPCODE_SHIFT = 24;
const RawInstruction IMMEDIATE_MASK = 0x00FF'FFFF;
const RawInstruction OPCODE_MASK = ~IMMEDIATE_MASK;

const std::unordered_map<int, const char*> TEST_NAMES_MAP({
												  { 1, "test_return_true"},
												  { 3, "test_return_false" },
												  { 4, "test_add" },
                          { 5, "test_sub" },
                          { 6, "test_mul" },
                          { 7, "test_div" },
                          { 9, "test_equal" }
  	  	  	  	  	  	  	  	  	  	  	  	});
// clang-format on

std::shared_ptr<ModuleMmap> _moduleMmap;

Om::ProcessRuntime runtime;

class InterpreterTest : public ::testing::Test {
 public:
  std::shared_ptr<Module> module_;

  virtual void SetUp() {
    // auto moduleName = getenv("B9_TEST_MODULE");
    // ASSERT_NE(moduleName, nullptr);

    char* moduleName = "test/interpreter_test.b9mod";

    std::ifstream file(moduleName, std::ios_base::in | std::ios_base::binary);

    module_ = b9::deserialize(file, _moduleMmap);
  }
};

TEST_F(InterpreterTest, interpreter) {
  Config cfg;

  VirtualMachine vm{runtime, cfg};
  vm.load(module_, _moduleMmap);

  for (auto test : TEST_NAMES) {
    EXPECT_TRUE(vm.run(test, {}).getInt48()) << "Test Failed: " << test;
  }
}

TEST_F(InterpreterTest, jit) {
  Config cfg;
  cfg.jit = true;

  VirtualMachine vm{runtime, cfg};
  vm.load(module_, _moduleMmap);
  vm.generateAllCode();

  for (auto test : TEST_NAMES) {
    EXPECT_TRUE(vm.run(test, {}).getInt48()) << "Test Failed: " << test;
  }
}

TEST_F(InterpreterTest, jit_dc) {
  Config cfg;
  cfg.jit = true;
  cfg.directCall = true;

  VirtualMachine vm{runtime, cfg};
  vm.load(module_, _moduleMmap);
  vm.generateAllCode();

  for (auto test : TEST_NAMES) {
    EXPECT_TRUE(vm.run(test, {}).getInt48()) << "Test Failed: " << test;
  }
}

TEST_F(InterpreterTest, jit_pp) {
  Config cfg;
  cfg.jit = true;
  cfg.directCall = true;
  cfg.passParam = true;

  VirtualMachine vm{runtime, cfg};
  vm.load(module_, _moduleMmap);
  vm.generateAllCode();

  for (auto test : TEST_NAMES) {
    EXPECT_TRUE(vm.run(test, {}).getInt48()) << "Test Failed: " << test;
  }
}

TEST_F(InterpreterTest, jit_lvms) {
  Config cfg;
  cfg.jit = true;
  cfg.directCall = true;
  cfg.passParam = true;
  cfg.lazyVmState = true;

  VirtualMachine vm{runtime, cfg};
  vm.load(module_, _moduleMmap);
  vm.generateAllCode();

  for (auto test : TEST_NAMES) {
    EXPECT_TRUE(vm.run(test, {}).getInt48()) << "Test Failed: " << test;
  }
}

TEST(MyTest, arguments) {
  Config cfg;
  cfg.jit = true;
  b9::VirtualMachine vm{runtime, cfg};

  auto m = std::make_shared<Module>();
  // std::vector<Instruction> i = {{OpCode::PUSH_FROM_PARAM, 0},
  //                               {OpCode::PUSH_FROM_PARAM, 1},
  //                               {OpCode::INT_ADD},
  //                               {OpCode::FUNCTION_RETURN},
  //                               END_SECTION};
  // m->functions.push_back(b9::FunctionDef{"add_args", i, 2, 0});

  auto module = std::make_shared<ModuleMmap>(128);
  std::string funcName = "add_args";
  module->insertNewFunc(funcName);
  module->insertParams(2,0); // nparams, nlocals
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_PARAM) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_PARAM) << OPCODE_SHIFT) | (1 & IMMEDIATE_MASK))); // push "Hello, World"
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::INT_ADD) << OPCODE_SHIFT));                           // GC. Object is kept alive by var0
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::DUPLICATE) << OPCODE_SHIFT));  
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PRIMITIVE_CALL) << OPCODE_SHIFT) | (1 & IMMEDIATE_MASK)));  // call b9_prim_print_number 
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::DROP) << OPCODE_SHIFT));  
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));

  vm.load(m, module);
  // auto r = vm.run("add_args", {{AS_INT48, 1}, {AS_INT48, 2}});
  auto r = vm.run(0, {{AS_INT48, 1}, {AS_INT48, 2}});
  EXPECT_EQ(r, Value(AS_INT48, 3));
}

TEST(MyTest, jitSimpleProgram) {
  Config cfg;
  cfg.jit = true;
  b9::VirtualMachine vm{runtime, cfg};
  auto m = std::make_shared<Module>();

  auto module = std::make_shared<ModuleMmap>(64);
  std::string funcName = "add";
  module->insertNewFunc(funcName);
  module->insertParams(0,0); // nparams, nlocals
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (0xdead & IMMEDIATE_MASK)));
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));

  // std::vector<Instruction> i = {{OpCode::INT_PUSH_CONSTANT, 0xdead},
  //                               {OpCode::FUNCTION_RETURN},
  //                               END_SECTION};
  // m->functions.push_back(b9::FunctionDef{"add", i, 0, 0});
  vm.load(m, module);
  vm.generateAllCode();
  // auto r = vm.run("add", {});
  auto r = vm.run(0, {});
  EXPECT_EQ(r, Value(AS_INT48, 0xdead));
}

// TODO: This is the same test as the one above
TEST(MyTest, haveAVariable) {
  Config cfg;
  cfg.jit = true;
  b9::VirtualMachine vm{runtime, cfg};
  auto m = std::make_shared<Module>();

  auto module = std::make_shared<ModuleMmap>(64);
  std::string funcName = "add";
  module->insertNewFunc(funcName);
  module->insertParams(0,0); // nparams, nlocals
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (0xdead & IMMEDIATE_MASK)));
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));

  // std::vector<Instruction> i = {{OpCode::INT_PUSH_CONSTANT, 0xdead},
  //                               {OpCode::FUNCTION_RETURN},
  //                               END_SECTION};
  // m->functions.push_back(b9::FunctionDef{"add", i, 0, 0});
  vm.load(m, module);
  vm.generateAllCode();
  // auto r = vm.run("add", {});
  auto r = vm.run(0, {});
  EXPECT_EQ(r, Value(AS_INT48, 0xdead));
}

TEST(ObjectTest, allocateSomething) {
  b9::VirtualMachine vm{runtime, {}};
  auto m = std::make_shared<Module>();

  auto module = std::make_shared<ModuleMmap>(128);
  std::string funcName = "allocate_object";
  module->insertNewFunc(funcName);
  module->insertParams(0,1); // nparams, nlocals
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::NEW_OBJECT) << OPCODE_SHIFT)); // new object
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::POP_INTO_LOCAL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::STR_PUSH_CONSTANT) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // push "Hello, World"
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_LOCAL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // push var0 aka object
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::POP_INTO_OBJECT) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // pop "Hello, World" into object at slot 0
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::SYSTEM_COLLECT) << OPCODE_SHIFT));                           // GC. Object is kept alive by var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_LOCAL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // push object
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_OBJECT) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // get the string back
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PRIMITIVE_CALL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));  // call b9_prim_print_string
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));
  module->recordStringSection(1); // There's one string to be inserted
  module->insertStringSection("Hello, World", 0); // Second parameter is the index associated with the string 

  // std::vector<Instruction> i = {
  //     {OpCode::NEW_OBJECT},            // new object
  //     {OpCode::POP_INTO_LOCAL, 0},     // store object into var0
  //     {OpCode::STR_PUSH_CONSTANT, 0},  // push "Hello, World"
  //     {OpCode::PUSH_FROM_LOCAL, 0},    // push var0 aka object
  //     {OpCode::POP_INTO_OBJECT, 0},  // pop "Hello, World" into object at slot 0
  //     {OpCode::SYSTEM_COLLECT},      // GC. Object is kept alive by var0
  //     {OpCode::PUSH_FROM_LOCAL, 0},  // push object
  //     {OpCode::PUSH_FROM_OBJECT, 0},  // get the string back
  //     {OpCode::PRIMITIVE_CALL, 0},    // call b9_prim_print_string
  //     {OpCode::FUNCTION_RETURN},      // finish with constant 0
  //     END_SECTION};
  // m->strings.push_back("Hello, World");
  // m->functions.push_back(b9::FunctionDef{"allocate_object", i, 0, 1});

  vm.load(m, module);
  // Value r = vm.run("allocate_object", {});
  Value r = vm.run(0, {});
  EXPECT_EQ(r, Value(AS_INT48, 0));
}

TEST(ObjectTest, allocateMultipleStrings) {
  b9::VirtualMachine vm{runtime, {}};
  auto m = std::make_shared<Module>();
  int stringCount = 3;

  auto module = std::make_shared<ModuleMmap>(256);
  std::string funcName = "allocate_object2";
  module->insertNewFunc(funcName);
  module->insertParams(0,stringCount); // nparams, nlocals

  for (int i = 0; i < stringCount; i++) {
    module->insertInstruction((instruction_type)(RawInstruction(OpCode::NEW_OBJECT) << OPCODE_SHIFT)); // new object
    module->insertInstruction((instruction_type)((RawInstruction(OpCode::POP_INTO_LOCAL) << OPCODE_SHIFT) | (i & IMMEDIATE_MASK))); // store object into var[i]
  }
  for (int i = 0; i < stringCount; i++) {
    module->insertInstruction((instruction_type)((RawInstruction(OpCode::STR_PUSH_CONSTANT) << OPCODE_SHIFT) | (i & IMMEDIATE_MASK))); // push string associated with i
    module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_LOCAL) << OPCODE_SHIFT) | (i & IMMEDIATE_MASK)));   // push var[i] aka object
    module->insertInstruction((instruction_type)((RawInstruction(OpCode::POP_INTO_OBJECT) << OPCODE_SHIFT) | (i & IMMEDIATE_MASK)));   // pop string associated with i into object at slot i
  }
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::SYSTEM_COLLECT) << OPCODE_SHIFT));  
  for (int i = 0; i < stringCount; i++)
  {
    module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_LOCAL) << OPCODE_SHIFT) | (i & IMMEDIATE_MASK))); // push object i
    module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_OBJECT) << OPCODE_SHIFT) | (i & IMMEDIATE_MASK))); // get the string i back
    module->insertInstruction((instruction_type)((RawInstruction(OpCode::PRIMITIVE_CALL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));  // call b9_prim_print_string
    if (i < stringCount - 1)
      module->insertInstruction((instruction_type)(RawInstruction(OpCode::DROP) << OPCODE_SHIFT)); // Drop return value from b9_prim_print_string
  }
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));
  module->recordStringSection(stringCount); // Specify the number of strings in string section
  module->insertStringSection("Hello, World", 0); // Second parameter is the index associated with the string 
  module->insertStringSection("Hello, Jurassic Park!", 1);
  module->insertStringSection("Good bye mad World!", 2);

  vm.load(m, module);
  Value r = vm.run(0, {});
  EXPECT_EQ(r, Value(AS_INT48, 0));
}

}  // namespace test
}  // namespace b9
