#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <set>
#include "common/source_file.h"
#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"

namespace chris {

struct CLIOptions {
    std::string command;
    std::string inputFile;
    bool jsonOutput = false;
    bool showHelp = false;
    bool showVersion = false;
};

CLIOptions parseArgs(int argc, char* argv[]) {
    CLIOptions opts;
    std::vector<std::string> args(argv + 1, argv + argc);

    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "--help" || args[i] == "-h") {
            opts.showHelp = true;
        } else if (args[i] == "--version" || args[i] == "-v") {
            opts.showVersion = true;
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            if (args[i + 1] == "json") {
                opts.jsonOutput = true;
            }
            i++;
        } else if (opts.command.empty()) {
            opts.command = args[i];
        } else if (opts.inputFile.empty()) {
            opts.inputFile = args[i];
        }
    }

    return opts;
}

void printUsage() {
    std::cout << "chrisplusplus compiler v0.1.0\n\n"
              << "Usage:\n"
              << "  chris build <file.chr>       Compile a .chr file to a native binary\n"
              << "  chris run <file.chr>         Build and run a .chr file\n"
              << "  chris test                   Run tests\n"
              << "  chris fmt <file.chr>         Format source code\n"
              << "  chris lint <file.chr>        Lint source code\n"
              << "  chris new <project>          Create a new project\n"
              << "\nOptions:\n"
              << "  --output json                Output diagnostics as JSON\n"
              << "  --help, -h                   Show this help\n"
              << "  --version, -v                Show version\n";
}

void printVersion() {
    std::cout << "chrisplusplus 0.1.0" << std::endl;
}

// Resolve directory from a file path
std::string dirName(const std::string& filePath) {
    auto pos = filePath.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return filePath.substr(0, pos);
}

// Recursively process imports: lex+parse imported .chr files and merge declarations
bool processImports(Program& program, const std::string& baseDir,
                    DiagnosticEngine& diagnostics, std::set<std::string>& imported) {
    // Collect import paths first (avoid modifying declarations while iterating)
    std::vector<std::string> importPaths;
    for (auto& decl : program.declarations) {
        if (auto* imp = dynamic_cast<ImportDecl*>(decl.get())) {
            std::string path = imp->path;
            // Only handle file imports (ending in .chr)
            if (path.size() >= 4 && path.substr(path.size() - 4) == ".chr") {
                // Resolve relative to base directory
                std::string fullPath = baseDir + "/" + path;
                if (imported.count(fullPath)) continue; // already imported
                importPaths.push_back(fullPath);
                imported.insert(fullPath);
            }
        }
    }

    // Process each import
    for (auto& fullPath : importPaths) {
        SourceFile importSource(fullPath);
        if (!importSource.load()) {
            diagnostics.error("E1001", "Could not open imported file: " + fullPath,
                             SourceLocation("<import>", 0, 0));
            return false;
        }

        Lexer importLexer(importSource.content(), fullPath, diagnostics);
        auto importTokens = importLexer.tokenize();
        if (diagnostics.hasErrors()) return false;

        Parser importParser(importTokens, diagnostics);
        auto importProgram = importParser.parse();
        if (diagnostics.hasErrors()) return false;

        // Recursively process imports in the imported file
        std::string importDir = dirName(fullPath);
        if (!processImports(importProgram, importDir, diagnostics, imported)) {
            return false;
        }

        // Merge imported declarations into the main program (prepend, so they're available)
        for (auto it = importProgram.declarations.rbegin();
             it != importProgram.declarations.rend(); ++it) {
            // Skip import declarations themselves
            if (dynamic_cast<ImportDecl*>(it->get())) continue;
            program.declarations.insert(program.declarations.begin(), std::move(*it));
        }
    }

    return true;
}

int buildCommand(const std::string& inputFile, bool jsonOutput) {
    DiagnosticEngine diagnostics;

    if (inputFile.empty()) {
        std::cerr << "error: no input file specified\n"
                  << "Usage: chris build <file.chr>" << std::endl;
        return 1;
    }

    // Check file extension
    if (inputFile.size() < 4 || inputFile.substr(inputFile.size() - 4) != ".chr") {
        std::cerr << "error: input file must have .chr extension: " << inputFile << std::endl;
        return 1;
    }

    // Load source file
    SourceFile source(inputFile);
    if (!source.load()) {
        std::cerr << "error: could not open file: " << inputFile << std::endl;
        return 1;
    }

    // Phase 1: Lex
    Lexer lexer(source.content(), inputFile, diagnostics);
    auto tokens = lexer.tokenize();

    if (diagnostics.hasErrors()) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Phase 2: Parse
    Parser parser(tokens, diagnostics);
    auto program = parser.parse();

    if (diagnostics.hasErrors()) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Phase 2.5: Process imports
    std::string baseDir = dirName(inputFile);
    std::set<std::string> imported;
    if (!processImports(program, baseDir, diagnostics, imported)) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Phase 3: Type Check
    TypeChecker checker(diagnostics);
    checker.check(program);

    if (diagnostics.hasErrors()) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Phase 4: Code Generation
    CodeGen codegen(inputFile, diagnostics);
    if (!codegen.generate(program, checker.genericInstantiations())) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Determine output paths
    std::string baseName = inputFile.substr(0, inputFile.size() - 4);
    std::string objectPath = baseName + ".o";
    std::string outputPath = baseName;

    // Emit object file
    if (!codegen.emitObjectFile(objectPath)) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Find runtime library — look relative to the compiler binary or in build dir
    std::string runtimePath;
    // Try common locations
    std::vector<std::string> runtimeSearchPaths = {
        "libchris_runtime.a",
        "build/libchris_runtime.a",
        "../build/libchris_runtime.a",
    };
    for (const auto& path : runtimeSearchPaths) {
        std::ifstream test(path);
        if (test.good()) {
            runtimePath = path;
            break;
        }
    }

    if (runtimePath.empty()) {
        std::cerr << "error: could not find chris runtime library (libchris_runtime.a)" << std::endl;
        std::cerr << "hint: build the project first with 'cmake --build build'" << std::endl;
        return 1;
    }

    // Link
    if (!codegen.linkExecutable(objectPath, runtimePath, outputPath)) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Clean up object file
    std::remove(objectPath.c_str());

    diagnostics.printAll(jsonOutput);
    std::cout << "Built: " << outputPath << std::endl;
    return 0;
}

} // namespace chris

int main(int argc, char* argv[]) {
    auto opts = chris::parseArgs(argc, argv);

    if (opts.showHelp || (opts.command.empty() && !opts.showVersion)) {
        chris::printUsage();
        return 0;
    }

    if (opts.showVersion) {
        chris::printVersion();
        return 0;
    }

    if (opts.command == "build") {
        return chris::buildCommand(opts.inputFile, opts.jsonOutput);
    } else if (opts.command == "run") {
        std::cerr << "error: 'run' command not yet implemented" << std::endl;
        return 1;
    } else if (opts.command == "test") {
        std::cerr << "error: 'test' command not yet implemented" << std::endl;
        return 1;
    } else if (opts.command == "fmt") {
        std::cerr << "error: 'fmt' command not yet implemented" << std::endl;
        return 1;
    } else if (opts.command == "lint") {
        std::cerr << "error: 'lint' command not yet implemented" << std::endl;
        return 1;
    } else if (opts.command == "new") {
        std::cerr << "error: 'new' command not yet implemented" << std::endl;
        return 1;
    } else {
        std::cerr << "error: unknown command '" << opts.command << "'\n"
                  << "Run 'chris --help' for usage." << std::endl;
        return 1;
    }
}
