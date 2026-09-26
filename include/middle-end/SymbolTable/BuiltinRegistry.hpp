#pragma once

#include "shared/structs.h"
#include <vector>
#include <map>
#include <functional>
#include <cstring>
#include <llvm-22/llvm/IR/Function.h>

/**
 * Callback type for the Interpreter.
 * Takes an array of SA::TypedVal and the count, returns a SA::TypedVal.
 */
using InterpreterCallback = std::function<SA::TypedVal(SA::TypedVal*, int)>;

struct CStringLess {
    bool operator()(const char* a, const char* b) const {
        return std::strcmp(a, b) < 0;
    }
};

struct BuiltinFunction {
    const char* name;
    SA::Type* return_type;
    std::vector<SA::Param*> param_types;
    
    // Interpreter implementation
    InterpreterCallback interpreter_impl;
    
    // LLVM cache (per-module)
    llvm::Function* llvm_func = nullptr;

    BuiltinFunction() : name(nullptr), return_type(nullptr), interpreter_impl(nullptr), llvm_func(nullptr) {}
};

class BuiltinRegistry {
public:
    static BuiltinRegistry& instance();

    // Maps a name to an implementation. Metadata is populated during bootstrap.
    void register_builtin(const char*, SA::Type*, std::vector<SA::Param*>, InterpreterCallback);
    BuiltinFunction* lookup(const char*);
    
    void bootstrap();

private:
    std::map<const char*, BuiltinFunction, CStringLess> registry;
};