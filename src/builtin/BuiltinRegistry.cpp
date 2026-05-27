#include "builtin/BuiltinRegistry.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include <iostream>
#include <cstdio>

#define macro(val, str)  if (val) { \
        printf((str), (val)); \
    } \
    return (TypedValue){.type = make_type(VOID, nullptr)};

// Example Interpreter implementation for SV_print_list
TypedValue SV_print_list_interpreter(TypedValue* args, int argc) {
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

void BuiltinRegistry::register_builtin(const char* name, Type_t* ret, std::vector<Type_t*> params, InterpreterCallback impl) {
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
    // Register SV_print_list: void SV_print_list(list[any], i32)
    // Using make_type to build the signature
    Type_t* void_ty = make_type(VOID, nullptr);
    Type_t* list_ty = make_type(LIST, make_type(UNKNOWN, nullptr)); // Generic list
    Type_t* i32_ty = make_type(I32, nullptr);

    register_builtin(
        "SV_print_list", 
        void_ty, 
        {list_ty, i32_ty}, 
        SV_print_list_interpreter
    );

    register_builtin(
        "printlns", 
        void_ty,
        { make_type(STRINGS, nullptr)}, 
        println_str
    );

    register_builtin(
        "printlni", 
        void_ty,
        { make_type(I128, nullptr)}, 
        printlni
    );

    register_builtin(
        "printlnf", 
        void_ty,
        { make_type(U128, nullptr)}, 
        printlnf
    );

    // Add more built-ins here (sin, cos, println, etc.)
}