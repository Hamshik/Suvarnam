#pragma once

#include "shared/structs.h"
#include <vector>
#include <map>
#include <functional>
#include <cstring>
#include <llvm-22/llvm/IR/Function.h>

/**
 * Callback type for the Interpreter.
 * Takes an array of TypedValue and the count, returns a TypedValue.
 */
using InterpreterCallback = std::function<TypedValue(TypedValue*, int)>;

struct CStringLess {
    bool operator()(const char* a, const char* b) const {
        return std::strcmp(a, b) < 0;
    }
};

struct BuiltinFunction {
    const char* name;
    Type_t* return_type;
    std::vector<Type_t*> param_types;
    
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
    void register_builtin(const char* name, Type_t* ret, std::vector<Type_t*> params, InterpreterCallback impl);
    BuiltinFunction* lookup(const char* name);
    
    void bootstrap(); 

private:
    std::map<const char*, BuiltinFunction, CStringLess> registry;
};