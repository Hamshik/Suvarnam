set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.15)
project(taca_compiler LANGUAGES C CXX)

# 1. Standard Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# LLVM's dependies
find_package(PkgConfig REQUIRED)
pkg_check_modules(zstd REQUIRED libzstd)
add_library(zstd::libzstd_shared INTERFACE IMPORTED)
set_target_properties(zstd::libzstd_shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${zstd_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${zstd_LIBRARIES}"
)

# 2. Locate LLVM 22 (Installed via apt)
# We hint the specific path for the LLVM cmake files
list(APPEND CMAKE_PREFIX_PATH "/usr/lib/llvm-22/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake from: ${LLVM_DIR}")

# 3. Include Directories
include_directories(include)
include_directories(src) # For some relative includes
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# 4. Find Bison and Flex
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# 5.1 Tell the compiler to look in the build folder for the generated parser.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(include)

# 5. Generate Parser and Lexer
# We generate them in the BUILD directory, not the SOURCE directory
BISON_TARGET(MyParser src/parser/parser.y 
             ${CMAKE_CURRENT_BINARY_DIR}/parser.c
             DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)

FLEX_TARGET(MyLexer src/lexer/lexer.l
            ${CMAKE_CURRENT_BINARY_DIR}/lexer.c)

ADD_FLEX_BISON_DEPENDENCY(MyLexer MyParser)

# 6. Collect all Source Files
# We use file(GLOB_RECURSE) to pick up all nested .c and .cpp files
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
    "src/*.c" 
    "src/*.cpp" 
    "TQlib/*.c"
)

# Remove the original parser.c and lexer.c if they exist in src to avoid double-definition
list(REMOVE_ITEM ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parser.c")
list(REMOVE_ITEM ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/lexer/lexer.c")

# 7. Define the Executable
add_executable(taca 
    ${ALL_SOURCES}
    ${BISON_MyParser_OUTPUTS}
    ${FLEX_MyLexer_OUTPUTS}
)

# 8. Map LLVM Components to Libraries
llvm_map_components_to_libnames(llvm_libs 
    core 
    analysis 
    native 
    bitreader 
    bitwriter 
    support 
    irreader
)

# 9. Linking
target_link_libraries(taca PRIVATE ${llvm_libs})

# Set output directory to bin/ as per your structure
set_target_properties(taca PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")