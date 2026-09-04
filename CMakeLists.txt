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

# 3. Locate Tools (Required to fix empty command invocations)
find_program(BISONCXX_EXECUTABLE bisonc++ REQUIRED)
find_program(FLEXCXX_EXECUTABLE flexc++ REQUIRED)

# ==============================================================================
# 4. Generate BisonC++ Parser
# ==============================================================================
set(PARSER_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/parser")

set(PARSER_H_INC     "${PARSER_INC_DIR}/Parser.h")
set(PARSER_IH_INC    "${PARSER_INC_DIR}/Parser.ih")

set(PARSER_CC        "${CMAKE_CURRENT_BINARY_DIR}/parse.cc")
set(PARSER_BASE      "${CMAKE_CURRENT_BINARY_DIR}/Parserbase.h")
set(PARSER_H_BUILD   "${CMAKE_CURRENT_BINARY_DIR}/Parser.h")
set(PARSER_IH_BUILD  "${CMAKE_CURRENT_BINARY_DIR}/Parser.ih")

file(MAKE_DIRECTORY "${PARSER_INC_DIR}")

# Step 1: Generate Parser files in build directory
add_custom_command(
    OUTPUT "${PARSER_CC}" "${PARSER_BASE}" "${PARSER_H_BUILD}" "${PARSER_IH_BUILD}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${PARSER_CC}" "${PARSER_BASE}" "${PARSER_H_BUILD}" "${PARSER_IH_BUILD}"
    COMMAND "${BISONCXX_EXECUTABLE}" --debug "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/parser.yy"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/parser.yy"
    COMMENT "Generating Parser via bisonc++"
)

# Step 2: Copy Parser headers to include/frontend/parser/
add_custom_command(
    OUTPUT "${PARSER_H_INC}" "${PARSER_IH_INC}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PARSER_H_BUILD}"  "${PARSER_H_INC}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PARSER_IH_BUILD}" "${PARSER_IH_INC}"
    DEPENDS "${PARSER_H_BUILD}" "${PARSER_IH_BUILD}"
    COMMENT "Copying parser headers to include/frontend/parser"
)

# ==============================================================================
# 5. Generate FlexC++ Lexer
# ==============================================================================
set(LEXER_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/frontend/lexer")

set(SCANNER_H_INC    "${LEXER_INC_DIR}/Scanner.h")
set(SCANNER_IH_INC   "${LEXER_INC_DIR}/Scanner.ih")

set(SCANNER_CC       "${CMAKE_CURRENT_BINARY_DIR}/Scanner.cc")
set(SCANNER_BASE     "${CMAKE_CURRENT_BINARY_DIR}/Scannerbase.h")
set(SCANNER_H_BUILD  "${CMAKE_CURRENT_BINARY_DIR}/Scanner.h")
set(SCANNER_IH_BUILD "${CMAKE_CURRENT_BINARY_DIR}/Scanner.ih")

file(MAKE_DIRECTORY "${LEXER_INC_DIR}")

# Step 1: Generate Lexer files in build directory
add_custom_command(
    OUTPUT "${SCANNER_CC}" "${SCANNER_BASE}" "${SCANNER_H_BUILD}" "${SCANNER_IH_BUILD}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${SCANNER_CC}" "${SCANNER_BASE}" "${SCANNER_H_BUILD}" "${SCANNER_IH_BUILD}"
    COMMAND "${FLEXCXX_EXECUTABLE}"
        --filenames=Scanner
        --lex-source="Scanner.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/lexer.l"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/lexer.l"
    COMMENT "Generating Scanner files via flexc++"
)

# Step 2: Copy Lexer headers to include/frontend/lexer/
add_custom_command(
    OUTPUT "${SCANNER_H_INC}" "${SCANNER_IH_INC}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SCANNER_H_BUILD}"  "${SCANNER_H_INC}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SCANNER_IH_BUILD}" "${SCANNER_IH_INC}"
    DEPENDS "${SCANNER_H_BUILD}" "${SCANNER_IH_BUILD}"
    COMMENT "Copying lexer headers to include/frontend/lexer"
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
    "${PARSER_H_INC}"
    "${PARSER_IH_INC}"
    "${SCANNER_CC}"
    "${SCANNER_H_INC}"
    "${SCANNER_IH_INC}"
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
    include/frontend/parser
    include/frontend/lexer
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