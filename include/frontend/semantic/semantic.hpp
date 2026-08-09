#pragma once

#include "shared/enums.h"
#include "shared/structs.h"
#include "SymbolTable/SymbolTable.hpp"

#ifdef __cplusplus

#include <llvm-22/llvm/IR/DerivedTypes.h>
#include <stdbool.h>
extern "C" {
#endif

void semantic_check(ASTNode_t *);
bool is_numeric(DataTypes_t);

#ifdef __cplusplus
}


extern "C" {
    Type_t* check_expr(ASTNode_t *, Type_t*&);
    Type_t* semantic_index_handle(ASTNode_t *);
    Type_t* list_handle(ASTNode_t *, Type_t* type = nullptr);
    bool islist(ASTNode_t *);
}
ASTNode_t* parse_file(FILE *);
Type_t* check_expr(ASTNode_t *);
bool fn_always_returns(ASTNode_t *);

extern bool isError;
extern size_t err_no;
extern size_t warn_no;
extern bool isWarning;
extern ASTNode_t *root;

void type_error(ASTNode_t *, const char *);
bool is_integer(DataTypes_t);
extern "C" void check_err();

Type_t* unop(ASTNode_t*, Type_t* type = nullptr);
Type_t* binop(ASTNode_t*, Type_t* type = nullptr);
Type_t* assign(ASTNode_t*, Type_t* type = nullptr);
void handle_idx_assign(ASTNode* &, ASTNode_t* &, Type_t* &);

Type_t* handle_fn(ASTNode_t*);
Type_t* ret(ASTNode_t *);
Type_t* call(ASTNode_t*);

void type_error(ASTNode_t *, const char*);
bool is_numeric(DataTypes_t);
DataTypes_t promote(DataTypes_t, DataTypes_t);
void force_numeric_type(ASTNode_t *, DataTypes_t);

bool literal_fits_type(const ASTNode_t *, DataTypes_t);
bool is_unsigned_numeric(DataTypes_t);
bool is_signed_numeric(DataTypes_t);
bool is_numeric(DataTypes_t);
bool is_integer(DataTypes_t);
int numeric_bits(DataTypes_t);
bool types_are_equal(Type_t*, Type_t*);
extern "C" Type_t* make_type(DataTypes_t, Type_t*);

void ensure_semantic(ASTModule_t *);

#endif
