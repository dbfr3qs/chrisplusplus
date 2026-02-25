cmake_minimum_required(VERSION 3.20)
project(chrisplusplus VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Compiler sources
set(CHRIS_SOURCES
    src/main.cpp
    src/common/source_location.cpp
    src/common/diagnostic.cpp
    src/common/source_file.cpp
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/ast/ast.cpp
    src/parser/parser.cpp
    src/sema/types.cpp
    src/sema/symbol_table.cpp
    src/sema/type_checker.cpp
    src/codegen/codegen.cpp
)

add_executable(chris ${CHRIS_SOURCES})

# Link LLVM
llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
    irreader
    native
    nativecodegen
    passes
    orcjit
    mc
    mcparser
    target
)
target_link_libraries(chris ${LLVM_LIBS})
target_include_directories(chris PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()

    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    # Test sources
    set(CHRIS_LIB_SOURCES
        src/common/source_location.cpp
        src/common/diagnostic.cpp
        src/common/source_file.cpp
        src/lexer/token.cpp
        src/lexer/lexer.cpp
        src/ast/ast.cpp
        src/parser/parser.cpp
        src/sema/types.cpp
        src/sema/symbol_table.cpp
        src/sema/type_checker.cpp
        src/codegen/codegen.cpp
    )

    add_library(chris_lib STATIC ${CHRIS_LIB_SOURCES})
    target_include_directories(chris_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(chris_lib ${LLVM_LIBS})

    add_executable(chris_tests
        tests/test_main.cpp
        tests/common/test_diagnostic.cpp
        tests/common/test_source_file.cpp
        tests/lexer/test_lexer.cpp
        tests/parser/test_parser.cpp
        tests/sema/test_type_checker.cpp
        tests/codegen/test_codegen.cpp
    )
    target_link_libraries(chris_tests chris_lib GTest::gtest GTest::gtest_main)
    target_include_directories(chris_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

    include(GoogleTest)
    gtest_discover_tests(chris_tests)

    # Runtime library (compiled as static lib for linking into compiled programs)
    add_library(chris_runtime STATIC runtime/runtime.c)
    target_include_directories(chris_runtime PUBLIC ${CMAKE_SOURCE_DIR}/runtime)
endif()

# Runtime library (always build)
if(NOT TARGET chris_runtime)
    add_library(chris_runtime STATIC runtime/runtime.c)
endif()
