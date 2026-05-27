# Suvarnam

The name **Suvarnam** is derived from the **Suvarna River**, a vital waterway flowing through the Tulu Nadu region (specifically Udupi/Odupi and Mangalore/Kudla) in Karnataka, India. Just as the river is a lifeline for the region, this language aims to provide a fluid yet structured foundation for high-performance computing.

Suvarnam is a strictly-typed, high-performance programming language built on top of the **LLVM** compiler infrastructure. It features a robust semantic analyzer capable of deep type inference, especially for nested list structures, and provides native support for modern string and numeric operations.

## Key Features

*   **LLVM Backend**: Compiles directly to efficient machine code via LLVM IR.
*   **Advanced Type Inference**: Supports auto-inference for variables (`var mut x = [[10, 10]]`) while maintaining strict fixed-size constraints.
*   **Recursive List Handling**: 
    *   Multi-dimensional nested lists (e.g., `[[i32; 2]; 1]`).
    *   **Size Locking**: Unsized list declarations lock to their specific dimensions and sizes upon initialization to prevent buffer overflows.
*   **String Repetition & Concatenation**: Native support for multiplying strings by integers (`"Hello" * 3`) and concatenating strings (`"a" + "b"`).
*   **Built-in Properties**: Compile-time `len()` property for fixed-size lists, injected directly as IR constants.
*   **Strict Type Safety**: Automatic width promotion for numeric types and explicit IR-level casting for function calls.

## Getting Started

### Prerequisites
*   LLVM 14+ 
*   CMake 3.10+
*   Flex & Bison
*   GCC/G++

### Building the Compiler
Use the provided build script to compile the project:

```bash
cmake --build build
```

### Compiling and Running Programs
To compile a `.tq` source file:

```bash
./bin/Complier source/test.tq
```

This will generate an executable in the `output/` directory.