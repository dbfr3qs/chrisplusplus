#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <set>
#include <dirent.h>
#include <sys/stat.h>
#include "common/source_file.h"
#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "fmt/formatter.h"

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

int runCommand(const std::string& inputFile, bool jsonOutput) {
    int buildResult = buildCommand(inputFile, jsonOutput);
    if (buildResult != 0) return buildResult;

    // Determine the executable path (same as build output)
    std::string baseName = inputFile.substr(0, inputFile.size() - 4);
    std::string execPath = baseName;

    // Make it a proper path if it's a relative name without directory
    if (execPath.find('/') == std::string::npos) {
        execPath = "./" + execPath;
    }

    std::cout << "Running: " << execPath << std::endl;
    std::cout << "---" << std::endl;
    int exitCode = std::system(execPath.c_str());

    // Clean up the binary after running
    std::remove(execPath.c_str());

    // std::system returns the full status; extract the exit code
    return WEXITSTATUS(exitCode);
}

int newCommand(const std::string& projectName) {
    if (projectName.empty()) {
        std::cerr << "error: no project name specified\n"
                  << "Usage: chris new <project>" << std::endl;
        return 1;
    }

    // Create project directory structure
    std::string mkdirCmd = "mkdir -p " + projectName + "/src " + projectName + "/test";
    if (std::system(mkdirCmd.c_str()) != 0) {
        std::cerr << "error: could not create project directory" << std::endl;
        return 1;
    }

    // Create main.chr
    std::string mainPath = projectName + "/src/main.chr";
    std::ofstream mainFile(mainPath);
    if (!mainFile.is_open()) {
        std::cerr << "error: could not create " << mainPath << std::endl;
        return 1;
    }
    mainFile << "func main() {\n"
             << "    print(\"Hello, world!\");\n"
             << "}\n";
    mainFile.close();

    // Create chris.toml
    std::string tomlPath = projectName + "/chris.toml";
    std::ofstream tomlFile(tomlPath);
    if (!tomlFile.is_open()) {
        std::cerr << "error: could not create " << tomlPath << std::endl;
        return 1;
    }
    tomlFile << "[package]\n"
             << "name = \"" << projectName << "\"\n"
             << "version = \"0.1.0\"\n"
             << "\n"
             << "[dependencies]\n";
    tomlFile.close();

    // Create test file
    std::string testPath = projectName + "/test/main_test.chr";
    std::ofstream testFile(testPath);
    if (!testFile.is_open()) {
        std::cerr << "error: could not create " << testPath << std::endl;
        return 1;
    }
    testFile << "// Tests for " << projectName << "\n"
             << "func test_example() {\n"
             << "    // TODO: add tests\n"
             << "}\n";
    testFile.close();

    std::cout << "Created project: " << projectName << std::endl;
    std::cout << "  " << projectName << "/chris.toml" << std::endl;
    std::cout << "  " << projectName << "/src/main.chr" << std::endl;
    std::cout << "  " << projectName << "/test/main_test.chr" << std::endl;
    std::cout << "\nTo build and run:\n"
              << "  cd " << projectName << "\n"
              << "  chris run src/main.chr" << std::endl;
    return 0;
}

// Discover test .chr files in a directory
std::vector<std::string> findTestFiles(const std::string& dir) {
    std::vector<std::string> files;
    DIR* d = opendir(dir.c_str());
    if (!d) return files;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        // Match test_*.chr or *_test.chr
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".chr") {
            if (name.substr(0, 5) == "test_" ||
                (name.size() >= 9 && name.substr(name.size() - 9) == "_test.chr")) {
                files.push_back(dir + "/" + name);
            }
        }
    }
    closedir(d);
    std::sort(files.begin(), files.end());
    return files;
}

// Extract names of test functions (func test_*) from a parsed program
std::vector<std::string> findTestFunctions(Program& program) {
    std::vector<std::string> names;
    for (auto& decl : program.declarations) {
        if (auto* func = dynamic_cast<FuncDecl*>(decl.get())) {
            if (func->name.substr(0, 5) == "test_") {
                names.push_back(func->name);
            }
        }
    }
    return names;
}

int testCommand(const std::string& inputFileOrDir, bool jsonOutput) {
    DiagnosticEngine diagnostics;
    std::vector<std::string> testFiles;

    if (!inputFileOrDir.empty() &&
        inputFileOrDir.size() >= 4 &&
        inputFileOrDir.substr(inputFileOrDir.size() - 4) == ".chr") {
        // Specific test file
        testFiles.push_back(inputFileOrDir);
    } else {
        // Search for test files in test/ directory
        std::string testDir = inputFileOrDir.empty() ? "test" : inputFileOrDir;
        testFiles = findTestFiles(testDir);
        if (testFiles.empty()) {
            std::cerr << "error: no test files found in '" << testDir << "/'\n"
                      << "hint: test files must match test_*.chr or *_test.chr" << std::endl;
            return 1;
        }
    }

    int totalPassed = 0;
    int totalFailed = 0;
    int totalTests = 0;

    for (auto& testFile : testFiles) {
        // Load and parse the test file to discover test functions
        SourceFile source(testFile);
        if (!source.load()) {
            std::cerr << "error: could not open test file: " << testFile << std::endl;
            totalFailed++;
            continue;
        }

        Lexer lexer(source.content(), testFile, diagnostics);
        auto tokens = lexer.tokenize();
        if (diagnostics.hasErrors()) {
            diagnostics.printAll(jsonOutput);
            totalFailed++;
            diagnostics = DiagnosticEngine(); // reset
            continue;
        }

        Parser parser(tokens, diagnostics);
        auto program = parser.parse();
        if (diagnostics.hasErrors()) {
            diagnostics.printAll(jsonOutput);
            totalFailed++;
            diagnostics = DiagnosticEngine();
            continue;
        }

        auto testNames = findTestFunctions(program);
        if (testNames.empty()) {
            std::cout << "  (no test functions in " << testFile << ")" << std::endl;
            continue;
        }

        // Generate a test harness source that includes the test file content
        // and a main() that calls each test function with reporting
        std::string harness = source.content();
        harness += "\n";
        harness += "// --- Generated test harness ---\n";
        harness += "extern func chris_test_start(name: String);\n";
        harness += "extern func chris_test_pass(name: String);\n";
        harness += "extern func chris_test_fail(name: String);\n";
        harness += "extern func chris_test_summary() -> Int;\n";
        harness += "func main() -> Int {\n";

        for (auto& name : testNames) {
            harness += "    chris_test_start(\"" + name + "\");\n";
            harness += "    try {\n";
            harness += "        " + name + "();\n";
            harness += "        chris_test_pass(\"" + name + "\");\n";
            harness += "    } catch (e: Error) {\n";
            harness += "        chris_test_fail(\"" + name + "\");\n";
            harness += "    }\n";
        }

        harness += "    return chris_test_summary();\n";
        harness += "}\n";

        // Now compile the harness
        DiagnosticEngine harnDiag;
        Lexer harnLexer(harness, testFile, harnDiag);
        auto harnTokens = harnLexer.tokenize();
        if (harnDiag.hasErrors()) {
            harnDiag.printAll(jsonOutput);
            totalFailed += (int)testNames.size();
            continue;
        }

        Parser harnParser(harnTokens, harnDiag);
        auto harnProgram = harnParser.parse();
        if (harnDiag.hasErrors()) {
            harnDiag.printAll(jsonOutput);
            totalFailed += (int)testNames.size();
            continue;
        }

        // Process imports
        std::string baseDir = dirName(testFile);
        std::set<std::string> imported;
        if (!processImports(harnProgram, baseDir, harnDiag, imported)) {
            harnDiag.printAll(jsonOutput);
            totalFailed += (int)testNames.size();
            continue;
        }

        TypeChecker checker(harnDiag);
        checker.check(harnProgram);
        if (harnDiag.hasErrors()) {
            harnDiag.printAll(jsonOutput);
            totalFailed += (int)testNames.size();
            continue;
        }

        CodeGen codegen(testFile, harnDiag);
        if (!codegen.generate(harnProgram, checker.genericInstantiations())) {
            harnDiag.printAll(jsonOutput);
            totalFailed += (int)testNames.size();
            continue;
        }

        // Emit object file
        std::string baseName = testFile.substr(0, testFile.size() - 4);
        std::string objectPath = baseName + "_test_harness.o";
        std::string outputPath = baseName + "_test_harness";

        if (!codegen.emitObjectFile(objectPath)) {
            totalFailed += (int)testNames.size();
            continue;
        }

        // Find runtime
        std::string runtimePath;
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
            std::cerr << "error: could not find chris runtime library" << std::endl;
            totalFailed += (int)testNames.size();
            continue;
        }

        // Link
        if (!codegen.linkExecutable(objectPath, runtimePath, outputPath)) {
            totalFailed += (int)testNames.size();
            std::remove(objectPath.c_str());
            continue;
        }
        std::remove(objectPath.c_str());

        // Run the test binary
        std::string execPath = outputPath;
        if (execPath.find('/') == std::string::npos) {
            execPath = "./" + execPath;
        }

        std::cout << "\n=== " << testFile << " (" << testNames.size() << " test(s)) ===" << std::endl;
        int exitCode = std::system(execPath.c_str());
        exitCode = WEXITSTATUS(exitCode);

        // The test binary returns the number of failures
        int passed = (int)testNames.size() - exitCode;
        if (passed < 0) passed = 0;
        totalPassed += passed;
        totalFailed += exitCode;
        totalTests += (int)testNames.size();

        // Clean up
        std::remove(outputPath.c_str());
    }

    // Final summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Total: " << totalTests << " test(s), "
              << totalPassed << " passed, "
              << totalFailed << " failed." << std::endl;

    return totalFailed > 0 ? 1 : 0;
}

int fmtCommand(const std::string& inputFile, bool jsonOutput) {
    DiagnosticEngine diagnostics;

    if (inputFile.empty()) {
        std::cerr << "error: no input file specified\n"
                  << "Usage: chris fmt <file.chr>" << std::endl;
        return 1;
    }

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

    // Parse
    Lexer lexer(source.content(), inputFile, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.hasErrors()) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse();
    if (diagnostics.hasErrors()) {
        diagnostics.printAll(jsonOutput);
        return 1;
    }

    // Format
    Formatter formatter;
    std::string formatted = formatter.format(program);

    // Write back to file
    std::ofstream out(inputFile);
    if (!out.is_open()) {
        std::cerr << "error: could not write to file: " << inputFile << std::endl;
        return 1;
    }
    out << formatted;
    out.close();

    std::cout << "Formatted: " << inputFile << std::endl;
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
        return chris::runCommand(opts.inputFile, opts.jsonOutput);
    } else if (opts.command == "test") {
        return chris::testCommand(opts.inputFile, opts.jsonOutput);
    } else if (opts.command == "fmt") {
        return chris::fmtCommand(opts.inputFile, opts.jsonOutput);
    } else if (opts.command == "lint") {
        std::cerr << "error: 'lint' command not yet implemented" << std::endl;
        return 1;
    } else if (opts.command == "new") {
        return chris::newCommand(opts.inputFile); // inputFile holds the project name
    } else {
        std::cerr << "error: unknown command '" << opts.command << "'\n"
                  << "Run 'chris --help' for usage." << std::endl;
        return 1;
    }
}
