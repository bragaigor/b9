#if !defined(B9_VALIDATOR_HPP_)
#define B9_VALIDATOR_HPP_

#include <b9/VirtualMachine.hpp>
// #include <b9/deserialize.hpp>

#include <iostream>

/// The b9run program's global configuration.
struct RunConfig {
  b9::Config b9;
  const char* moduleName = "";
  const char* mainFunction = "<script>";
  bool verbose = false;
  std::vector<b9::StackElement> usrArgs;
};

namespace b9 {

    enum CurrentBytecode {
        HEADER, 
        FUNCTION_SECTION,
            FUNCTION_SECTION_CODE,
            FUNCTION_SECTION_BODY,
                FUNCTION_NAME,
        STRING_SECTION,
            STRING_SECTION_CODE,
            STRING_SECTION_BODY,
        SECTION_BODY, 
        SECTION_CODE, 
        FUCNTION,
        INSTRUCTION
    };

    struct ValidatorException : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class Validator {

        public:

        Validator(const RunConfig& cfg);
        bool validate();

        private:

        inline bool readBytes(std::istream &in, char *buffer, long bytes) {
            long count = 0;
            do {
                in.read(&buffer[count], bytes - count);
                count += in.gcount();
            } while (count < bytes && in.good());
            return count == bytes;
        }

        template <typename Number>
        bool readNumber(std::istream &in, Number &out, long bytes = sizeof(Number)) {
            char *buffer = (char *)&out;
            return readBytes(in, buffer, bytes);
        }

        // TODO: Reuse some of the functions in deserialize.hpp? Or not?
        void readSection(std::istream &in);
        void readString(std::istream &in, std::string &toRead);
        void readFunctionData(std::istream &in);
        void readFunctionSection(std::istream &in);
        void readStringSection(std::istream &in);

        RunConfig cfg_;

    };

} // namespace b9

#endif