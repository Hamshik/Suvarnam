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
    return (TypedValue){.type = make_type(VOID, nullptr)};

// Example Interpreter implementation for SA_print_list
TypedValue SA_print_list_interpreter(TypedValue* args, int argc) {
    if (argc < 2) return (TypedValue){0};
    // Implementation logic here...
    std::cout << "[Interpreter] Printing list at " << args[0].val.raw << std::endl;
    return (TypedValue){.type = make_type(VOID, nullptr)};
}

TypedValue println_str(TypedValue* str, int argc) {
   macro(str->val.chars, "%s\n");
}

TypedValue printlni(TypedValue* ival, int argc) {
   macro(ival->val.i64, "%ld\n");
}

TypedValue printlnf(TypedValue* fval, int argc) {
   macro(fval->val.f128, "%Lf\n");
}

BuiltinRegistry& BuiltinRegistry::instance() {
    static BuiltinRegistry inst;
    return inst;
}

void BuiltinRegistry::register_builtin(const char* name, Type_t* ret, std::vector<Param_t*> params, InterpreterCallback impl) {
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
    // Using make_type to build the signature
    Type_t* void_ty = make_type(VOID, nullptr);

    register_builtin(
        "malloc", 
        make_type(PTR, void_ty),
        { new Param_t(make_type(U128, nullptr)) }, 
        nullptr
    );

    register_builtin(
        "printf", 
        void_ty,
        {
            new Param(make_type(STRINGS, nullptr)),
            new Param(true, make_type(UNKNOWN, nullptr))
        }, 
        nullptr
    );
    
        // Provide a runtime helper that returns a null-terminated UTF-8 byte
        // sequence for a single character at `idx` inside a string.
        register_builtin(
            "_SA_getCharAt",
            make_type(PTR, nullptr),
            {
                new Param(make_type(STRINGS, nullptr)),
                new Param(make_type(I64, nullptr))
            },
            nullptr
        );

    // Encode a Unicode code point (CHARACTER) into a UTF-8 byte sequence
    register_builtin(
        "SA_encode_cp",
        make_type(PTR, nullptr),
        { new Param(make_type(CHARACTER, nullptr)) },
        nullptr
    );

    // Add more built-ins here (sin, cos, println, etc.)
}