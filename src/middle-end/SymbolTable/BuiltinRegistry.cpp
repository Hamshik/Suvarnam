#include "SymbolTable/BuiltinRegistry.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <iostream>
#include <cstdio>
#include <stdlib.h>


#define macro(val, str)  if (val) { \
        printf((str), (val)); \
    } \
    return (SA::TypedVal){.type = new SA::Type(VOID, nullptr)};

// Example Interpreter implementation for SA_print_list
SA::TypedVal SA_print_list_interpreter(SA::TypedVal* args, int argc) {
    if (argc < 2) return (SA::TypedVal){0};
    // Implementation logic here...
    std::cout << "[Interpreter] Printing list at " << args[0].val.raw << std::endl;
    return (SA::TypedVal){.type = new SA::Type(VOID, nullptr)};
}

SA::TypedVal println_str(SA::TypedVal* str, int argc) {
   macro(str->val.chars, "%s\n");
}

SA::TypedVal printlni(SA::TypedVal* ival, int argc) {
   macro(ival->val.i64, "%ld\n");
}

SA::TypedVal printlnf(SA::TypedVal* fval, int argc) {
   macro(fval->val.f128, "%Lf\n");
}

BuiltinRegistry& BuiltinRegistry::instance() {
    static BuiltinRegistry inst;
    return inst;
}

void BuiltinRegistry::register_builtin(const char* name, SA::Type* ret, std::vector<SA::Param*> params, InterpreterCallback impl) {
    BuiltinFunction fn;
    fn.name = name;
    fn.return_type = ret;
    fn.param_types = params;
    fn.interpreter_impl = impl;
    registry[name] = fn;
}

BuiltinFunction* BuiltinRegistry::lookup(const char* name) {
    if (!name) return nullptr;
    auto it = registry.find(name);
    if (it == registry.end()) return nullptr;
    return &it->second;
}

void BuiltinRegistry::bootstrap() {
    // Register SA_print_list: void SA_print_list(list[any], i32)
    // Using new SA::Type to build the signature
    SA::Type* void_ty = new SA::Type(VOID, nullptr);

    register_builtin(
        "malloc", 
        new SA::Type(PTR, void_ty),
        { new SA::Param(new SA::Type(U128, nullptr)) }, 
        nullptr
    );

    register_builtin(
        "printf", 
        void_ty,
        {
            new SA::Param(new SA::Type(STRINGS, nullptr)),
            new SA::Param(true, new SA::Type(UNKNOWN, nullptr))
        }, 
        nullptr
    );
    
        // Provide a runtime helper that returns a null-terminated UTF-8 byte
        // sequence for a single character at `idx` inside a string.
        register_builtin(
            "_SA_getCharAt",
            new SA::Type(PTR, nullptr),
            {
                new SA::Param(new SA::Type(STRINGS, nullptr)),
                new SA::Param(new SA::Type(I64, nullptr))
            },
            nullptr
        );

    // Encode a Unicode code point (CHARACTER) into a UTF-8 byte sequence
    register_builtin(
        "SA_encode_cp",
        new SA::Type(PTR, nullptr),
        { new SA::Param(new SA::Type(CHARACTER, nullptr)) },
        nullptr
    );

    // Add more built-ins here (sin, cos, println, etc.)
}