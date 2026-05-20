#include "codegen.hpp"
#include "driver.hpp"
#include "sema.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string inputPath;
    std::string objectPath;
    std::string irPath;
    std::string targetTriple = "x86_64-pc-linux-gnu";
    bool showHelp = false;
};

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

void printUsage(std::ostream& stream) {
    stream << "usage: mini-cc input.mc -o output.o [--emit-ir output.ll] "
              "[--target x86_64-pc-linux-gnu]\n";
}

bool parseArgs(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
            return true;
        }

        if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "error: -o requires a path\n";
                return false;
            }
            options.objectPath = argv[++i];
            continue;
        }

        if (arg == "--emit-ir") {
            if (i + 1 >= argc) {
                std::cerr << "error: --emit-ir requires a path\n";
                return false;
            }
            options.irPath = argv[++i];
            continue;
        }

        if (startsWith(arg, "--emit-ir=")) {
            options.irPath = arg.substr(std::string("--emit-ir=").size());
            continue;
        }

        if (arg == "--target") {
            if (i + 1 >= argc) {
                std::cerr << "error: --target requires a target triple\n";
                return false;
            }
            options.targetTriple = argv[++i];
            continue;
        }

        if (startsWith(arg, "--target=")) {
            options.targetTriple = arg.substr(std::string("--target=").size());
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return false;
        }

        if (!options.inputPath.empty()) {
            std::cerr << "error: multiple input files are not supported\n";
            return false;
        }
        options.inputPath = arg;
    }

    if (options.inputPath.empty()) {
        std::cerr << "error: input file is required\n";
        return false;
    }
    if (options.objectPath.empty()) {
        std::cerr << "error: output object path is required; pass -o output.o\n";
        return false;
    }

    return true;
}

void printErrors(const std::vector<std::string>& errors) {
    for (const std::string& error : errors) {
        std::cerr << error << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArgs(argc, argv, options)) {
        printUsage(std::cerr);
        return 1;
    }

    if (options.showHelp) {
        printUsage(std::cout);
        return 0;
    }

    mini::Driver driver;
    if (!driver.parseFile(options.inputPath)) {
        printErrors(driver.errors());
        return 1;
    }

    std::unique_ptr<mini::Program> program = driver.takeProgram();
    mini::SemanticAnalyzer sema;
    if (!sema.analyze(*program)) {
        printErrors(sema.errors());
        return 1;
    }

    mini::CodeGenerator codegen(options.targetTriple);
    if (!codegen.generate(*program, options.objectPath, options.irPath)) {
        printErrors(codegen.errors());
        return 1;
    }

    std::cout << "wrote object file: " << options.objectPath << '\n';
    return 0;
}
