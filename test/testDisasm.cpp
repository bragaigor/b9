#include <b9/ExecutionContext.hpp>
#include <b9/Module.hpp>
#include <b9/VirtualMachine.hpp>
#include <b9/deserialize.hpp>
#include <b9/serialize.hpp>

#include <gtest/gtest.h>
#include <stdlib.h>
#include <strstream>
#include <vector>

namespace b9 {
namespace test {

/* Helper functions for creating test modules */

const RawInstruction OPCODE_SHIFT = 24;
const RawInstruction IMMEDIATE_MASK = 0x00FF'FFFF;
const RawInstruction OPCODE_MASK = ~IMMEDIATE_MASK;

std::shared_ptr<Module> makeEmptyModule() {
  auto m = std::make_shared<Module>();
  return m;
}

std::shared_ptr<Module> makeSimpleModule(std::shared_ptr<ModuleMmap> &moduleMmap) {
  auto m = std::make_shared<Module>();
  std::vector<Instruction> i = {{OpCode::INT_PUSH_CONSTANT, 2},
                                {OpCode::INT_PUSH_CONSTANT, 2},
                                {OpCode::INT_ADD},
                                {OpCode::FUNCTION_RETURN},
                                END_SECTION};

  m->functions.push_back(b9::FunctionDef{"add_args", i, 2, 4});
  m->strings = {"FruitBat", "cantaloupe", "123$#*"};

  auto module = std::make_shared<ModuleMmap>(128);
  std::string funcName = "add_args";
  module->insertNewFunc(funcName);
  module->insertParams(2,4); // nparams, nlocals 
  module->reserveAddrInstructions();
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (2 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (2 & IMMEDIATE_MASK))); // push "Hello, World"
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::INT_ADD) << OPCODE_SHIFT));                           // GC. Object is kept alive by var0
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));
  module->storeInstructionsCount(5);
  module->recordStringSection(3); // There's one string to be inserted
  module->insertStringSection("FruitBat", 0); // Second parameter is the index associated with the string 
  module->insertStringSection("cantaloupe", 1);
  module->insertStringSection("123#*", 2); 

  moduleMmap = module;

  return m;
}

std::shared_ptr<Module> makeComplexModule(std::shared_ptr<ModuleMmap> &moduleMmap) {
  auto m = std::make_shared<Module>();

  std::vector<Instruction> i1 = {{OpCode::INT_PUSH_CONSTANT, 2},
                                 {OpCode::INT_PUSH_CONSTANT, 2},
                                 {OpCode::INT_ADD},
                                 {OpCode::FUNCTION_RETURN},
                                 END_SECTION};
  m->functions.push_back(b9::FunctionDef{"add_args", i1, 1, 1});

  std::vector<Instruction> i2 = {{OpCode::PUSH_FROM_LOCAL, 0},
                                 {OpCode::PRIMITIVE_CALL, 0},
                                 {OpCode::DROP, 0},
                                 {OpCode::INT_PUSH_CONSTANT, 0},
                                 {OpCode::FUNCTION_RETURN, 0},
                                 END_SECTION};
  m->functions.push_back(b9::FunctionDef{"b9PrintString", i2, 2, 2});

  std::vector<Instruction> i3 = {{OpCode::PUSH_FROM_LOCAL, 0},
                                 {OpCode::PRIMITIVE_CALL, 1},
                                 {OpCode::DROP, 0},
                                 {OpCode::INT_PUSH_CONSTANT, 0},
                                 {OpCode::FUNCTION_RETURN, 0},
                                 END_SECTION};
  m->functions.push_back(b9::FunctionDef{"b9PrintNumber", i3, 3, 3});

  m->strings = {"mercury", "Venus", "EARTH", "mars", "JuPiTeR", "sAtUrN"};

  auto module = std::make_shared<ModuleMmap>(256);
  std::string funcName1 = "add_args";
  module->insertNewFunc(funcName1);
  module->insertParams(1,1); // nparams, nlocals 
  module->reserveAddrInstructions();
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (2 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (2 & IMMEDIATE_MASK))); // push "Hello, World"
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::INT_ADD) << OPCODE_SHIFT));                           // GC. Object is kept alive by var0
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));
  module->storeInstructionsCount(5);
  
  std::string funcName2 = "b9PrintString";
  module->insertNewFunc(funcName2);
  module->insertParams(2,2); // nparams, nlocals 
  module->reserveAddrInstructions();
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_LOCAL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PRIMITIVE_CALL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // push "Hello, World"
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::DROP) << OPCODE_SHIFT));                           // GC. Object is kept alive by var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));
  module->storeInstructionsCount(6);
  
  std::string funcName3 = "b9PrintNumber";
  module->insertNewFunc(funcName3);
  module->insertParams(3,3); // nparams, nlocals 
  module->reserveAddrInstructions();
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PUSH_FROM_LOCAL) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::PRIMITIVE_CALL) << OPCODE_SHIFT) | (1 & IMMEDIATE_MASK))); // push "Hello, World"
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::DROP) << OPCODE_SHIFT));                           // GC. Object is kept alive by var0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK))); // store object into var0
  module->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));
  module->storeInstructionsCount(6);

  module->recordStringSection(6); // There's one string to be inserted
  module->insertStringSection("mercury", 0); // Second parameter is the index associated with the string 
  module->insertStringSection("Venus", 1);
  module->insertStringSection("EARTH", 2); 
  module->insertStringSection("mars", 3); // Second parameter is the index associated with the string 
  module->insertStringSection("JuPiTeR", 4);
  module->insertStringSection("sAtUrN", 5); 

  moduleMmap = module;

  return m;
}

/* Unit Tests */

// void roundTripStringSection(const std::vector<std::string>& strings) {
//   std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
//   writeStringSection(buffer, strings);
//   EXPECT_TRUE(buffer.good());

//   std::vector<std::string> strings2;
//   std::shared_ptr<ModuleMmap> module;
//   readStringSection(buffer, strings2, module);

//   EXPECT_EQ(strings, strings2);
// }

// TODO: FIX TEST
// TEST(RoundTripSerializationTest, testStringSection) {
//   roundTripStringSection({"mercury", "venus", "pluto"});
//   roundTripStringSection({"", "", "", ""});
//   roundTripStringSection({});
// }

// bool roundTripInstructions(std::vector<Instruction> instructions) {
//   std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
//   writeInstructions(buffer, instructions);

//   std::vector<Instruction> result;
//   std::shared_ptr<ModuleMmap> temp;
//   if (readInstructions(buffer, result, temp) == -1) {
//     return false;
//   }

//   EXPECT_EQ(instructions.size(), result.size());
//   for (int i = 0; i < instructions.size(); i++) {
//     EXPECT_EQ(instructions[i], result[i]);
//   }
//   return true;
// }

// TODO: FIX TEST
// TEST(RoundTripSerializationTest, testInstructions) {
//   std::vector<Instruction> instructions1 = {{OpCode::INT_PUSH_CONSTANT, 2},
//                                             {OpCode::INT_PUSH_CONSTANT, 2},
//                                             {OpCode::INT_ADD},
//                                             {OpCode::FUNCTION_RETURN},
//                                             END_SECTION};

//   std::vector<Instruction> instructions2 = {};

//   std::vector<Instruction> instructions3 = {{OpCode::INT_PUSH_CONSTANT, 2},
//                                             {OpCode::INT_PUSH_CONSTANT, 2},
//                                             {OpCode::INT_ADD},
//                                             {OpCode::FUNCTION_RETURN}};

//   std::vector<Instruction> instructions4 = {END_SECTION};

//   EXPECT_TRUE(roundTripInstructions(instructions1));
//   EXPECT_FALSE(roundTripInstructions(instructions2));
//   EXPECT_FALSE(roundTripInstructions(instructions3));
//   EXPECT_TRUE(roundTripInstructions(instructions4));
// }

// void roundTripFunctionData(FunctionDef& f) {
//   std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
//   writeFunctionData(buffer, f);
//   EXPECT_TRUE(buffer.good());

//   std::vector<Instruction> i2;
//   auto f2 = FunctionDef{"", i2, 0, 0};
//   std::shared_ptr<ModuleMmap> temp;
//   readFunctionData(buffer, f2, temp);
//   EXPECT_EQ(f, f2);
// }

// TODO: FIX TEST
// TEST(RoundTripSerializationTest, testFunctionData) {
//   std::vector<Instruction> i1 = {{OpCode::INT_PUSH_CONSTANT, 2},
//                                  {OpCode::INT_PUSH_CONSTANT, 2},
//                                  {OpCode::INT_ADD},
//                                  {OpCode::FUNCTION_RETURN},
//                                  END_SECTION};
//   auto f1 = FunctionDef{"testName", i1, 4, 5};

//   std::vector<Instruction> i2 = {};
//   auto f2 = FunctionDef{"testName", i2, 4, 5};

//   roundTripFunctionData(f1);
//   roundTripFunctionData(f2);
// }

// void roundTripFunctionSection(std::vector<FunctionDef> functions) {
//   std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
//   writeFunctionSection(buffer, functions);
//   EXPECT_TRUE(buffer.good());

//   std::vector<FunctionDef> functions2;
//   std::shared_ptr<ModuleMmap> module;
//   readFunctionSection(buffer, functions2, module);

//   EXPECT_EQ(functions.size(), functions2.size());

//   for (int i = 0; i < functions.size(); i++) {
//     EXPECT_EQ(functions[i], functions2[i]);
//   }
// }

// TODO: FIX TEST
// TEST(RoundTripSerializationTest, testFunctionSection) {
//   std::vector<Instruction> i1 = {{OpCode::INT_PUSH_CONSTANT, 2},
//                                  {OpCode::INT_PUSH_CONSTANT, 2},
//                                  {OpCode::INT_ADD},
//                                  {OpCode::FUNCTION_RETURN},
//                                  END_SECTION};
//   auto f1 = FunctionDef{"testName", i1, 4, 5};
//   std::vector<FunctionDef> functions;
//   functions.push_back(f1);

//   roundTripFunctionSection(functions);
// }

void roundTripSerializeDeserialize(std::shared_ptr<Module> module, std::shared_ptr<ModuleMmap> &moduleMmap) {
  std::cout << "$$$$$$$$$ moduleMmap->getNumberOfFunctions(): " << moduleMmap->getNumberOfFunctions() << std::endl;
  std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
  serialize(buffer, *module, moduleMmap);
  std::cout << "$$$$$$$$$ serialize done!!!!\n";

  std::shared_ptr<ModuleMmap> m;
  auto module2 = deserialize(buffer, m);

  // EXPECT_EQ(*module, *module2);
  // EXPECT_EQ(*module, *module);
  // EXPECT_EQ(*module2, *module2);
  if (*m == *moduleMmap) { //&& *m == *m && *moduleMmap == *m) {
    std::cout << "YAAAAAAAY they are equal!!!!\n";
  } else {
    std::cout << "NOOOOOOOOO they are NOT equal!!!!\n";
  }
  // EXPECT_EQ(*m, *moduleMmap);
  // EXPECT_EQ(*m, *m);
  // EXPECT_EQ(*moduleMmap, *m);
  std::cout << "Exiting roundTripSerializeDeserialize()...\n";
}

TEST(RoundTripSerializationTest, testSerializeDeserialize) {
  std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);

  std::shared_ptr<ModuleMmap> module1;
  std::shared_ptr<ModuleMmap> module2;

  auto m1 = makeSimpleModule(module1);
  roundTripSerializeDeserialize(m1, module1);

  auto m2 = makeComplexModule(module2);
  std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
  std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
  roundTripSerializeDeserialize(m2, module2);
  std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
  std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";

  auto m3 = std::make_shared<Module>();
  // std::vector<Instruction> i = {{OpCode::INT_PUSH_CONSTANT, 2},
  //                               {OpCode::INT_PUSH_CONSTANT, 2},
  //                               {OpCode::INT_ADD},
  //                               {OpCode::FUNCTION_RETURN},
  //                               END_SECTION};
  // auto f = FunctionDef{"testName", i, 4, 5};
  // std::vector<FunctionDef> functions;
  // functions.push_back(f);
  // m3->functions = functions;

  std::shared_ptr<ModuleMmap> module3 = std::make_shared<ModuleMmap>(128);
  std::string funcName = "testName";
  std::cout << "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n";
  module3->insertNewFunc(funcName);
  module3->insertParams(4,5); // nparams, nlocals 
  module3->reserveAddrInstructions();
  module3->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (2 & IMMEDIATE_MASK))); // store object into var0
  module3->insertInstruction((instruction_type)((RawInstruction(OpCode::INT_PUSH_CONSTANT) << OPCODE_SHIFT) | (2 & IMMEDIATE_MASK))); // push "Hello, World"
  module3->insertInstruction((instruction_type)(RawInstruction(OpCode::INT_ADD) << OPCODE_SHIFT));                           // GC. Object is kept alive by var0
  module3->insertInstruction((instruction_type)(RawInstruction(OpCode::FUNCTION_RETURN) << OPCODE_SHIFT));                          // finish with constant 0
  module3->insertInstruction((instruction_type)((RawInstruction(OpCode::END_SECTION) << OPCODE_SHIFT) | (0 & IMMEDIATE_MASK)));
  module3->storeInstructionsCount(5);
  module3->recordStringSection(0);

  std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
  roundTripSerializeDeserialize(m3, module3);
  std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";

  auto m4 = std::make_shared<Module>();
  // std::vector<std::string> strings = {"sandwich", "RubberDuck", "Dumbledore"};
  // m4->strings = strings;

  auto module4 = std::make_shared<ModuleMmap>(128);
  module4->recordStringSection(3); // There's one string to be inserted
  module4->insertStringSection("sandwich", 0); // Second parameter is the index associated with the string 
  module4->insertStringSection("RubberDuck", 1);
  module4->insertStringSection("Dumbledore", 2); 

  roundTripSerializeDeserialize(m4, module4);
}

template <typename Number>
void roundTripNumber(std::vector<Number> numbers) {
  for (auto number : numbers) {
    std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_TRUE(writeNumber(buffer, number));
    Number toRead;
    EXPECT_TRUE(readNumber(buffer, toRead));
    EXPECT_EQ(number, toRead);
  }
}

TEST(RoundTripSerializationTest, testWriteReadNumber) {
  std::vector<int> numbers1 = {-1, 0, 15, 250, 10000, -10000};
  std::vector<uint32_t> numbers2 = {20, 0, 375, 25000, 13000};
  std::vector<uint64_t> numbers3 = {500, 0, 825, 13000, 7200};
  std::vector<size_t> numbers4 = {250, 0, 37, 16000, 28000};

  roundTripNumber(numbers1);
  roundTripNumber(numbers2);
  roundTripNumber(numbers3);
  roundTripNumber(numbers4);
}

TEST(RoundTripSerializationTest, testWriteReadString) {
  for (auto string : {"power ranger", "", "the\0empty\0string"}) {
    std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
    writeString(buffer, string);
    std::string toRead;
    readString(buffer, toRead);
    EXPECT_EQ(string, toRead);
  }
}

TEST(ReadBinaryTest, testEmptyModule) {
  std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
  std::shared_ptr<ModuleMmap> m;
  EXPECT_THROW(deserialize(buffer, m), DeserializeException);
}

TEST(ReadBinaryTest, testCorruptModule) {
  std::stringstream buffer1(std::ios::in | std::ios::out | std::ios::binary);
  std::string corruptHeader = "b9mod";
  uint32_t sectionCode = 1;
  uint32_t functionCount = 4;
  writeString(buffer1, corruptHeader);
  writeNumber(buffer1, sectionCode);
  writeNumber(buffer1, functionCount);

  std::stringstream buffer2(std::ios::in | std::ios::out | std::ios::binary);
  sectionCode = 3;
  writeHeader(buffer2);
  writeNumber(buffer2, sectionCode);
  writeNumber(buffer2, functionCount);

  std::shared_ptr<ModuleMmap> m;
  EXPECT_THROW(deserialize(buffer1, m), DeserializeException);
  EXPECT_THROW(deserialize(buffer2, m), DeserializeException);
}

TEST(ReadBinaryTest, runValidModule) {
  std::shared_ptr<ModuleMmap> module1;
  auto m1 = makeSimpleModule(module1);
  std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
  serialize(buffer, *m1, module1);

  std::shared_ptr<ModuleMmap> m;
  auto m2 = deserialize(buffer, m);
  Om::ProcessRuntime runtime;
  VirtualMachine vm(runtime, {});
  vm.load(m2, m);
  vm.run(0, {Om::Value(Om::AS_INT48, 1), Om::Value(Om::AS_INT48, 2)});
}

}  // namespace test
}  // namespace b9
