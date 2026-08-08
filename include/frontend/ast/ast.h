#pragma once

#include "shared/enums.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "shared/structs.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Constructors */
ASTNode_t *new_num(char *, DataTypes_t, SA_Location);
ASTNode_t *new_str(char *, SA_Location);
ASTNode_t *new_char_bytes(const char *, size_t, SA_Location);
ASTNode_t *new_var(const char *, DataTypes_t, SA_Location);
ASTNode_t *new_binop(ASTNode_t *, ASTNode_t *, SA_Location, OP_kind_t);
ASTNode_t *new_unop(ASTNode_t *, SA_Location, OP_kind_t);
ASTNode_t *new_assign(ASTNode_t *, ASTNode_t *, Type_t *, bool, SA_Location, OP_kind_t);
ASTNode_t *new_if(ASTNode_t *, ASTNode_t *, ASTNode_t *, SA_Location);
ASTNode_t *new_for(const char *, ASTNode_t *, ASTNode_t *, SA_Location, bool);
ASTNode_t *new_seq(ASTNode_t *, ASTNode_t *);
ASTNode_t *new_while(ASTNode_t *, ASTNode_t *, ASTNode_t *, SA_Location);
ASTNode_t *new_bool(bool, SA_Location);
ASTNode_t *new_fn_def(const char *, Param_t *, int, Type_t *, ASTNode_t *, SA_Location);
ASTNode_t *new_fn_call(const char *, ASTNode_t *, SA_Location);
ASTNode_t *new_return(ASTNode_t *, SA_Location);
ASTNode_t *new_import_node(const char *, SA_Location);
ASTNode_t *new_list(ASTNode_t *, SA_Location);
ASTNode_t *new_index(ASTNode_t *, idx_expr_t *, bool, SA_Location);
ASTNode_t *new_range(ASTNode_t *, ASTNode_t *, ASTNode_t *, bool);
ASTNode_t *new_break(SA_Location);
ASTNode_t *new_continue(SA_Location);


/* Eval + memory */
void ast_free(ASTNode_t *n);
ASTNode_t *ast_alloc(void);
Type_t* make_type(DataTypes_t base, Type_t* inner);

/* Env */
void set_var(const char *, SA_Value *, Type_t *);
void set_var_current(const char *, SA_Value *, DataTypes_t);
SA_Value getvar(const char *, Type_t *, SA_Location);
void env_push(void);
void env_pop(void);
void env_clear_all(void);
void assign_value(DataTypes_t, SA_Value *, SA_Value);
SA_Value eval_assign(ASTNode_t *, ASTNode_t *, OP_kind_t, Type_t *, SA_Location);
TypedValue *getvar_ref(const char *, SA_Location);
int env_frame_id_of(const char *, SA_Location);
TypedValue *getvar_ref_at(int, const char *, SA_Location);
void set_var_at(int, const char *, SA_Value *, Type_t *, SA_Location);

#ifdef __cplusplus
}
#endif