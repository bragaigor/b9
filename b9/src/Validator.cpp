#include <string.h>
#include <cstring>
#include <memory>

#include <b9/Validator.hpp>

namespace b9 {

    Validator::Validator(const RunConfig &cfg) : cfg_(cfg) {
        std::printf("Calling Validator Constructor...\n");
    }

    bool Validator::validate() {
        std::printf("Calling Validator::validate...\n");

        std::ifstream file(cfg_.moduleName, std::ios_base::in | std::ios_base::binary);

        uint32_t moduleSize;
        readNumber(file, moduleSize);

        while (file.peek() != std::istream::traits_type::eof()) {
            readSection(file);
        }

        return true;
    }

    void Validator::readString(std::istream &in, std::string &toRead) {
        uint32_t length;
        if (!readNumber(in, length, sizeof(length))) {
            throw ValidatorException{"Error reading string length"};
        }
        for (size_t i = 0; i < length; i++) {
            if (in.eof()) {
                throw ValidatorException{"Error reading string"};
            }
            char current = in.get();
            toRead.push_back(current);
        }
    }

    void Validator::readFunctionData(std::istream &in) {
        std::string name;
        uint32_t nparams;
        uint32_t nlocals;

        readString(in, name);
        bool ok = readNumber(in, nparams) &&
                    readNumber(in, nlocals);
        if (!ok) {
            throw ValidatorException{"Error reading function data"};
        }
    }

    void Validator::readFunctionSection(std::istream &in) {
        uint32_t functionCount;
        if (!readNumber(in, functionCount)) {
            throw ValidatorException{"Error reading function count"};
        }
        for (uint32_t i = 0; i < functionCount; i++) {
            // TODO: Implement proper bytecode check
            // functions.emplace_back(FunctionDef{"", std::vector<Instruction>{}, 0, 0});
            readFunctionData(in);
        }
    }

    void Validator::readStringSection(std::istream &in) {
        uint32_t stringCount;
        if (!readNumber(in, stringCount)) {
            throw ValidatorException{"Error reading string count"};
        }
        for (uint32_t i = 0; i < stringCount; i++) {
            std::string toRead;
            readString(in, toRead);
            // strings.push_back(toRead);
        }
    }

    void Validator::readSection(std::istream &in) {
        uint32_t sectionCode;
        if (!readNumber(in, sectionCode)) {
            throw ValidatorException{"Validator: Error reading section code"};
        }

        switch (sectionCode) {
            case 1:
            return readFunctionSection(in);
            case 2:
            return readStringSection(in);
            default:
            throw ValidatorException{"Validator: Invalid Section Code"};
        }
    }

} // namespace b9