cmake_minimum_required(VERSION 3.16)

project(mini_compiler VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BISON REQUIRED)
if(BISON_VERSION VERSION_LESS 3.5)
    message(FATAL_ERROR "GNU Bison 3.5 or newer is required for the C++ parser skeleton")
endif()

find_package(FLEX REQUIRED)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

BISON_TARGET(
    MiniParser
    "${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y"
    "${GENERATED_DIR}/parser.cpp"
    DEFINES_FILE "${GENERATED_DIR}/parser.hpp"
)

FLEX_TARGET(
    MiniLexer
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l"
    "${GENERATED_DIR}/lexer.cpp"
)

ADD_FLEX_BISON_DEPENDENCY(MiniLexer MiniParser)

add_executable(mini-cc
    src/main.cpp
    src/driver.cpp
    src/ast.cpp
    src/sema.cpp
    src/codegen.cpp
    ${BISON_MiniParser_OUTPUTS}
    ${FLEX_MiniLexer_OUTPUTS}
)

target_include_directories(mini-cc PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GENERATED_DIR}"
)

target_include_directories(mini-cc SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(mini-cc PRIVATE ${LLVM_DEFINITIONS_LIST})

if(MSVC)
    target_compile_options(mini-cc PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(mini-cc PRIVATE -Wall -Wextra)
endif()

if(LLVM_LINK_LLVM_DYLIB)
    target_link_libraries(mini-cc PRIVATE LLVM)
else()
    llvm_map_components_to_libnames(LLVM_LIBS
        Core
        Support
        Target
        MC
        CodeGen
        X86CodeGen
        X86AsmParser
        X86Desc
        X86Info
    )
    target_link_libraries(mini-cc PRIVATE ${LLVM_LIBS})
endif()

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
    add_custom_target(mini-check
        COMMAND "${BASH_PROGRAM}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_suite.sh"
                "$<TARGET_FILE:mini-cc>"
                "${CMAKE_CURRENT_BINARY_DIR}/test-output"
        DEPENDS mini-cc
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Running mini compiler valid/invalid test suite"
    )
endif()
