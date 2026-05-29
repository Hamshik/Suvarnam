#pragma once

#include "shared/enums.h"
#include "shared/structs.h"
#include "SymbolTable/SymbolTable.hpp"

#ifdef __cplusplus

#include <llvm-22/llvm/IR/DerivedTypes.h>
#include <stdbool.h>
extern "C" {
#endif

void semantic_check(ASTNode_t *root);
bool is_numeric(DataTypes_t t);

#ifdef __cplusplus
}


extern "C" {
    Type_t* check_expr(ASTNode_t *n, Type_t*& type);
    Type_t* semantic_index_handle(ASTNode_t *n);
    Type_t* list_handle(ASTNode_t *n, Type_t* type = nullptr);
    bool islist(ASTNode_t *target);
}
Type_t* check_expr(ASTNode_t *n);

extern bool isError;
extern size_t err_no;
extern size_t warn_no;
extern bool isWarning;
extern ASTNode_t *root;

void type_error(ASTNode_t *n, const char *msg);
bool is_integer(DataTypes_t t);
void check_err();

Type_t* unop(ASTNode_t* n, Type_t* type = nullptr);
Type_t* binop(ASTNode_t* n, Type_t* type = nullptr);
Type_t* assign(ASTNode_t* n, Type_t* type = nullptr);
void handle_idx_assign(ASTNode* &n, ASTNode_t* &lhs, Type_t* &type);

Type_t* handle_fn(ASTNode_t* n);
Type_t* ret(ASTNode_t *n);
Type_t* call(ASTNode_t* n);

void type_error(ASTNode_t *n,const char* msg);
bool is_numeric(DataTypes_t t);
DataTypes_t promote(DataTypes_t a, DataTypes_t b);
void force_numeric_type(ASTNode_t *n, DataTypes_t t);

bool literal_fits_type(const ASTNode_t *lit, DataTypes_t t);
bool is_unsigned_numeric(DataTypes_t t);
bool is_signed_numeric(DataTypes_t t);
bool is_numeric(DataTypes_t t);
bool is_integer(DataTypes_t t);
int numeric_bits(DataTypes_t t);
bool types_are_equal(Type_t* a, Type_t* b);
extern "C" Type_t* make_type(DataTypes_t base, Type_t* inner);

void ensure_semantic(Module_t *m);

#endif
