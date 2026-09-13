#pragma once

#include "shared/enums.h"

#include "shared/structs.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Constructors */
ASTNode *new_num(const char *, DataTypes_t, SA_Location);
ASTNode *new_str(char *, SA_Location);
ASTNode *new_char_bytes(const char *, size_t, SA_Location);
ASTNode *new_var(const char *, DataTypes_t, SA_Location);
ASTNode *new_binop(ASTNode *, ASTNode *, SA_Location, OP_kind_t);
ASTNode *new_unop(ASTNode *, SA_Location, OP_kind_t);
ASTNode *new_assign(ASTNode *, ASTNode *, TypeInfo *, bool, SA_Location, OP_kind_t);
ASTNode *new_if(ASTNode *, ASTNode *, ASTNode *, SA_Location);
ASTNode *new_for(const char *, ASTNode *, ASTNode *, SA_Location, bool);
ASTNode *new_seq(ASTNode *, ASTNode *);
ASTNode *new_while(ASTNode *, ASTNode *, ASTNode *, SA_Location);
ASTNode *new_bool(bool, SA_Location);
ASTNode *new_fn_def(const char *, Param_t *, int, TypeInfo *, ASTNode *, SA_Location);
ASTNode *new_fn_call(const char *, ASTNode *, SA_Location);
ASTNode *new_return(ASTNode *, SA_Location);
ASTNode *new_import_node(const char *, SA_Location);
ASTNode *new_list(ASTNode *, SA_Location);
ASTNode *new_index(ASTNode *, idx_expr_t *, bool, SA_Location);
ASTNode *new_range(ASTNode *, ASTNode *, ASTNode *, bool);
ASTNode *new_break(SA_Location);
ASTNode *new_continue(SA_Location);


void ast_free(ASTNode *n);
ASTNode *ast_alloc(void);
TypeInfo* make_type(DataTypes_t base, TypeInfo* inner);
