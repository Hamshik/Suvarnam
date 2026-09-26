#pragma once

#include "shared/enums.h"

#include "shared/structs.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Constructors */
ASTNode *new_num(const char *, DataTypes_t, SA::Location);
ASTNode *new_str(char *, SA::Location);
ASTNode *new_char_bytes(const char *, size_t, SA::Location);
ASTNode *new_var(const char *, DataTypes_t, SA::Location);
ASTNode *new_binop(ASTNode *, ASTNode *, SA::Location, OP_kind_t);
ASTNode *new_unop(ASTNode *, SA::Location, OP_kind_t);
ASTNode *new_assign(ASTNode *, ASTNode *, SA::Type *, bool, SA::Location, OP_kind_t);
ASTNode *new_if(ASTNode *, ASTNode *, ASTNode *, SA::Location);
ASTNode *new_for(const char *, ASTNode *, ASTNode *, SA::Location, bool);
ASTNode *new_seq(ASTNode *, ASTNode *);
ASTNode *new_while(ASTNode *, ASTNode *, ASTNode *, SA::Location);
ASTNode *new_bool(bool, SA::Location);
ASTNode *new_fn_def(const char *, SA::Param *, int, SA::Type *, ASTNode *, SA::Location);
ASTNode *new_fn_call(const char *, ASTNode *, SA::Location);
ASTNode *new_return(ASTNode *, SA::Location);
ASTNode *new_import_node(const char *, SA::Location);
ASTNode *new_list(ASTNode *, SA::Location);
ASTNode *new_index(ASTNode *, SA::idxExpr *, bool, SA::Location);
ASTNode *new_range(ASTNode *, ASTNode *, ASTNode *, bool);
ASTNode *new_break(SA::Location);
ASTNode *new_continue(SA::Location);


void ast_free(ASTNode *n);
ASTNode *ast_alloc(void);
SA::Type* make_type(DataTypes_t base, SA::Type* inner);
