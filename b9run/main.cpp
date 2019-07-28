#include <b9/ExecutionContext.hpp>
#include <b9/compiler/Compiler.hpp>
#include <b9/deserialize.hpp>
#include <b9/Validator.hpp>

#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Runtime.hpp>

#include <strings.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

/// B9run's usage string. Printed when run with -help.
static const char* usage =
    "Usage: b9run [<option>...] [--] <module> [<arg>...]\n"
    "   Or: b9run -help\n"
    "Jit Options:\n"
    "  -jit:          Enable the jit\n"
    "  -directcall:   make direct jit to jit calls\n"
    "  -passparam:    Pass arguments in CPU registers\n"
    "  -lazyvmstate:  Only update the VM state as needed\n"
    "Run Options:\n"
    "  -inline <n>:   Set the jit's max inline depth (default: 0)\n"
    "  -debug:        Enable debug code\n"
    "  -verbose:      Run with verbose printing\n"
    "  -help:         Print this help message";

std::ostream& operator<<(std::ostream& out, const RunConfig& cfg) {
  out << "Module:       " << cfg.moduleName << std::endl;

  out << "Arguments:    [ ";
  for (const auto& arg : cfg.usrArgs) {
    out << arg << " ";
  }
  out << "]" << std::endl;

  out << cfg.b9;
  return out;
}

/// Parse CLI arguments and set up the config.
static bool parseArguments(RunConfig& cfg, const int argc, char* argv[]) {
  std::size_t i = 1;

  for (; i < argc; i++) {
    const char* arg = argv[i];

    if (strcasecmp(arg, "-help") == 0) {
      std::cout << usage << std::endl;
      exit(EXIT_SUCCESS);
    } else if (strcasecmp(arg, "-inline") == 0) {
      cfg.b9.maxInlineDepth = atoi(argv[++i]);
    } else if (strcasecmp(arg, "-verbose") == 0) {
      cfg.verbose = true;
      cfg.b9.verbose = true;
    } else if (strcasecmp(arg, "-debug") == 0) {
      cfg.b9.debug = true;
    } else if (strcasecmp(arg, "-jit") == 0) {
      cfg.b9.jit = true;
    } else if (strcasecmp(arg, "-directcall") == 0) {
      cfg.b9.directCall = true;
    } else if (strcasecmp(arg, "-passparam") == 0) {
      cfg.b9.passParam = true;
    } else if (strcasecmp(arg, "-lazyvmstate") == 0) {
      cfg.b9.lazyVmState = true;
    } else if (strcmp(arg, "--") == 0) {
      i++;
      break;
    } else if (strcmp(arg, "-") == 0) {
      std::cerr << "Unrecognized option: " << arg << std::endl;
      return false;
    } else {
      break;
    }
  }

  // check for user defined module
  if (i < argc) {
    cfg.moduleName = argv[i++];
  } else {
    std::cerr << "No module name given to b9run" << std::endl;
    return false;
  }

  // check for user defined arguments
  for (; i < argc; i++) {
    cfg.usrArgs.push_back(Om::Value(Om::AS_INT48, std::atoi(argv[i])));
  }

  // check that dependent options are enabled
  if (cfg.b9.directCall && !cfg.b9.jit) {
    std::cerr << "-directcall requires -jit" << std::endl;
    return false;
  }
  if (cfg.b9.passParam && !cfg.b9.directCall) {
    std::cerr << "-passparam requires -directcall" << std::endl;
    return false;
  }
  if (cfg.b9.lazyVmState && !cfg.b9.passParam) {
    std::cerr << "-lazyvmstate requires -passparam" << std::endl;
    return false;
  }

  return true;
}

static void run(Om::ProcessRuntime& runtime, const RunConfig& cfg) {
  b9::VirtualMachine vm{runtime, cfg.b9};

  std::ifstream file(cfg.moduleName, std::ios_base::in | std::ios_base::binary);
  std::ifstream file_temp(cfg.moduleName, std::ios_base::binary | std::ios_base::ate); // TODO: Is this worth it to grab file size?
  std::shared_ptr<ModuleMmap> moduleMmap;
  auto module = b9::deserialize(file, moduleMmap);
  std::cout << "\tFile read has size: " << file_temp.tellg() << std::endl;
  vm.load(module, moduleMmap);

  if (cfg.b9.jit) {
    vm.generateAllCode();
  }

  std::cout << "\tNumber of functions from new module: " << moduleMmap->getNumberOfFunctions() << std::endl;

  size_t functionIndex = module->getFunctionIndex(cfg.mainFunction);
  // auto result = vm.run(functionIndex, cfg.usrArgs);
  auto result = vm.run(cfg.mainFunction, cfg.usrArgs);
  std::cout << std::endl << "=> " << result << std::endl;
}

int main(int argc, char* argv[]) {
  Om::ProcessRuntime runtime;
  RunConfig cfg;

  if (!parseArguments(cfg, argc, argv)) {
    std::cerr << usage << std::endl;
    exit(EXIT_FAILURE);
  }

  if (cfg.verbose) {
    std::cout << cfg << std::endl << std::endl;
    std::cout << "Validating bytecodes..." << std::endl;
  }

  // Validator validator{cfg};
  // if (!validator.validate()) {
  //   std::cerr << "Bytecodes failed validation. Exiting..." << std::endl;
  //   exit(EXIT_FAILURE);
  // } else {
  //     if (cfg.verbose) {
  //     std::cout << "Bytecodes validated succcessfully." << std::endl;
  //   }
  // }

  try {
    run(runtime, cfg);
  } catch (const b9::DeserializeException& e) {
    std::cerr << "Failed to load module: " << e.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch (const b9::FunctionNotFoundException& e) {
    std::cerr << "Failed to find function: " << e.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch (const b9::BadFunctionCallException& e) {
    std::cerr << "Failed to call function " << e.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch (const b9::CompilationException& e) {
    std::cerr << "Failed to compile function: " << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
