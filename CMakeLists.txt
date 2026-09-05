cmake_minimum_required(VERSION 3.15)
project(suvarnam_compiler LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Standard Settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

# LLVM's dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(zstd REQUIRED libzstd)
add_library(zstd::libzstd_shared INTERFACE IMPORTED)
set_target_properties(zstd::libzstd_shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${zstd_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${zstd_LIBRARIES}"
)

# 2. Locate LLVM 22
list(APPEND CMAKE_PREFIX_PATH "/usr/lib/llvm-22/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake from: ${LLVM_DIR}")

# 3. Locate Tools
find_program(BISONCXX_EXECUTABLE bisonc++ REQUIRED)
find_program(FLEXCXX_EXECUTABLE flexc++ REQUIRED)

# ==============================================================================
# 4. Generate BisonC++ Parser
# ==============================================================================
set(PARSER_CC        "${CMAKE_CURRENT_BINARY_DIR}/parse.cc")
set(PARSER_BASE      "${CMAKE_CURRENT_BINARY_DIR}/Parserbase.h")
set(PARSER_H_BUILD   "${CMAKE_CURRENT_BINARY_DIR}/Parser.h")
set(PARSER_IH_BUILD  "${CMAKE_CURRENT_BINARY_DIR}/Parser.ih")
# Tell CMake that Parser.h is generated at build time
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/Parser.h
    ${CMAKE_CURRENT_BINARY_DIR}/Parser.ih
    PROPERTIES GENERATED TRUE
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/parse.cc" 
           "${CMAKE_CURRENT_BINARY_DIR}/Parserbase.h"
    # Execute ln -sf via shell to forcefully create symlinks
    COMMAND /bin/sh -c "ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/Parser.h ${CMAKE_CURRENT_BINARY_DIR}/Parser.h"
    COMMAND /bin/sh -c "ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/Parser.ih ${CMAKE_CURRENT_BINARY_DIR}/Parser.ih"
    # Run bisonc++
    COMMAND "${BISONCXX_EXECUTABLE}" --debug "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/parser.yy"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/parser.yy"
    COMMENT "Symlinking Parser headers and running bisonc++"
)

# ==============================================================================
# 5. Generate FlexC++ Lexer
# ==============================================================================
set(SCANNER_CC       "${CMAKE_CURRENT_BINARY_DIR}/Scanner.cc")
set(SCANNER_BASE     "${CMAKE_CURRENT_BINARY_DIR}/Scannerbase.h")
set(SCANNER_H_BUILD  "${CMAKE_CURRENT_BINARY_DIR}/Scanner.h")
set(SCANNER_IH_BUILD "${CMAKE_CURRENT_BINARY_DIR}/Scanner.ih")

# Tell CMake that Parser.h is generated at build time
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/Scanner.h
    ${CMAKE_CURRENT_BINARY_DIR}/Scanner.ih
    PROPERTIES GENERATED TRUE
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Scanner.cc" 
           "${CMAKE_CURRENT_BINARY_DIR}/Scannerbase.h"
    # Execute ln -sf via shell to forcefully create symlinks
    COMMAND /bin/sh -c "ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/Scanner.h ${CMAKE_CURRENT_BINARY_DIR}/Scanner.h"
    COMMAND /bin/sh -c "ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/Scanner.ih ${CMAKE_CURRENT_BINARY_DIR}/Scanner.ih"
    # Run flexc++
    COMMAND "${FLEXCXX_EXECUTABLE}" --filenames=Scanner --lex-source="Scanner.cc" "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/lexer.l"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/lexer.l"
    COMMENT "Symlinking Scanner headers and running flexc++"
)

# ==============================================================================
# 6. Define Executable
# ==============================================================================
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
    "src/*.c" 
    "src/*.cpp" 
)

add_executable(suvarnam 
    ${ALL_SOURCES}
    "${PARSER_CC}"
    "${PARSER_H_BUILD}"
    "${PARSER_IH_BUILD}"
    "${SCANNER_CC}"
    "${SCANNER_H_BUILD}"
    "${SCANNER_IH_BUILD}"
)

set_source_files_properties(
    src/frontend/lexer/helpers.c
    PROPERTIES LANGUAGE CXX
)

# 8. Include Directories Configuration
target_include_directories(suvarnam PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}
    include/backend
    include/middle-end
    include/frontend
)

target_include_directories(suvarnam SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(suvarnam PRIVATE ${LLVM_DEFINITIONS})

# 9. Map LLVM Components
llvm_map_components_to_libnames(llvm_libs 
    core 
    analysis 
    native 
    bitreader 
    bitwriter 
    support 
    irreader
)

# 10. Linking
target_link_libraries(suvarnam PRIVATE ${llvm_libs} zstd::libzstd_shared)

# Set runtime output directory
set_target_properties(suvarnam PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
