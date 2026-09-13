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
set(PARSER_CC   "${CMAKE_CURRENT_BINARY_DIR}/parse.cc")
set(PARSER_BASE "${CMAKE_CURRENT_BINARY_DIR}/Parserbase.h")

file(GLOB GRAMMAR_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/parser.yy"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser/*.ly"
)

add_custom_command(
    OUTPUT "${PARSER_CC}" "${PARSER_BASE}"
    COMMAND bash -e "${CMAKE_CURRENT_SOURCE_DIR}/setup_link.sh"
    COMMAND "${BISONCXX_EXECUTABLE}" --debug
        --parsefun-source=parse.cc
        --baseclass-header=Parserbase.h
        parser.yy
    DEPENDS
        ${GRAMMAR_FILES}
    COMMENT "Running bisonc++"
)

# ==============================================================================
# 5. Generate FlexC++ Lexer
# ==============================================================================
set(SCANNER_CC   "${CMAKE_CURRENT_BINARY_DIR}/Scanner.cc")
set(SCANNER_BASE "${CMAKE_CURRENT_BINARY_DIR}/Scannerbase.h")

add_custom_command(
    OUTPUT "${SCANNER_CC}" "${SCANNER_BASE}"
    COMMAND "${FLEXCXX_EXECUTABLE}" --filenames=Scanner
        --lex-source="Scanner.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/lexer.l"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer/lexer.l"
    COMMENT "Running flexc++"
)

# ==============================================================================
# 6. Define Executable
# ==============================================================================
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

list(REMOVE_ITEM ALL_SOURCES)

add_executable(suvarnam
    ${ALL_SOURCES}
    "${PARSER_CC}"
    "${SCANNER_CC}"
)

# ==============================================================================
# 7. Include Directories
# ==============================================================================
target_include_directories(suvarnam PRIVATE
    include
    src/frontend/parser        # ← your hand-written Parser.h / Parser.ih
    src/frontend/lexer         # ← your hand-written Scanner.h / Scanner.ih
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
