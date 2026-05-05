#include "shared/structs.h"
#include "eval/eval.h"
#include "utils/error_handler/error.h"
#include "shared/structs.h"
#include "ast/ast.h"
#include <stdlib.h>
#include <string.h>

extern file_t file;

void assign_value(DataTypes_t dt,  TQValue *dst,  TQValue src) {
    switch (dt) {
        case I8:     dst->i8 = src.i8; break;
        case I16:    dst->i16 = src.i16; break;
        case I32:    dst->i32 = src.i32; break;
        case I64:    dst->i64 = src.i64; break;
        case I128:   dst->i128 = src.i128; break;
        case U8:     dst->u8 = src.u8; break;
        case U16:    dst->u16 = src.u16; break;
        case U32:    dst->u32 = src.u32; break;
        case U64:    dst->u64 = src.u64; break;
        case U128:   dst->u128 = src.u128; break;
        case F32:    dst->f32 = src.f32; break;
        case F64:    dst->f64 = src.f64; break;
        case F128:   dst->f128 = src.f128; break;
        case UF32:   dst->f32 = src.f32; break;
        case UF64:   dst->f64 = src.f64; break;
        case UF128:  dst->f128 = src.f128; break;
        case BOOL:   dst->bval = src.bval; break;
        case STRINGS:
            if(dst != NULL && dst->str) free(dst->str);
            dst->str = strdup(src.str);
            break;
        case CHARACTER:
            dst->chars = src.chars;
            break;
        
        case PTR: {
            free(dst->ptr.name);
            dst->ptr.frame_id = src.ptr.frame_id;
            dst->ptr.name = src.ptr.name ? strdup(src.ptr.name) : NULL;
            if (src.ptr.name && !dst->ptr.name) { perror("strdup"); exit(1); }
            break;
        }
        case LIST:
            dst->raw = src.raw;
            break;
        default:
            fprintf(stderr, "Invalid assignment type\n");
            exit(1);
    }
}

 TQValue eval_assign(ASTNode_t *lhs, ASTNode_t *rhs, OP_kind_t op, DataTypes_t datatypes , TQLocation loc) {
    TypedValue rt0 = ast_eval(rhs);
    TypedValue rt = TQcast_typed(rt0, datatypes);
    TQValue r = rt.val;
    TQValue v = {0};

    if (!lhs) {
        panic(&file, loc, RT_ASSIGN_TARGET_NOT_VAR, NULL);
        return ( TQValue){0};
    }

    /* Assignment to variable */
    if (lhs->kind == AST_VAR) {
        if (op == OP_ASSIGN) {
            set_var(lhs->var, &r, datatypes);
            return r;
        }

       TQValue cur = getvar(lhs->var, datatypes, loc);
        OP_kind_t operation = get_assign_op(op);
        switch (datatypes) {
            case I8: case I16: case I32: case I128:
            case U8: case U16: case U32: case U64: case U128:
            case F32: case F64: case F128:
            case UF32: case UF64: case UF128:
                v = TQeval_binop_numeric(operation, datatypes, cur, r);
                break;
            case BOOL:
                v = eval_bool(operation, BOOL, cur, r);
                break;
            case STRINGS:
                v = ( TQValue){.str = do_operation_str(cur.str, r.str, operation)};
                break;
            case CHARACTER:
                v.chars = r.chars;
                break;
            case PTR:
                panic(&file, loc, RT_ASSIGN_UNSUPPORTED, "pointer compound assignment not supported");
                return ( TQValue){0};
            default:
                panic(&file, loc, RT_ASSIGN_UNSUPPORTED, NULL);
                return ( TQValue){0};
        }
        set_var(lhs->var, &v, datatypes);
        return v;
    }

    // TypedValue right_val = ast_eval(rhs);

    /* Assignment through dereference: *p = rhs */
    if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
        TypedValue pv = ast_eval(lhs->unop.operand);
        if (pv.type != PTR || pv.val.ptr.name == NULL) {
            panic(&file, loc, RT_DANGLING_PTR, NULL);
            return ( TQValue){0};
        }
        TypedValue *target = getvar_ref_at(pv.val.ptr.frame_id, pv.val.ptr.name, loc);
        if (!target) return ( TQValue){0};
        if (target->type != datatypes) {
            panic(&file, loc, RT_VAR_TYPE_MISMATCH, pv.val.ptr.name);
            return ( TQValue){0};
        }

        if (op == OP_ASSIGN) {
            assign_value(datatypes, &target->val, r);
            return r;
        }

       TQValue cur = target->val;
        OP_kind_t operation = get_assign_op(op);
        switch (datatypes) {
            case I8: case I16: case I32: case I128:
            case U8: case U16: case U32: case U64: case U128:
            case F32: case F64: case F128:
            case UF32: case UF64: case UF128:
                v = TQeval_binop_numeric(operation, datatypes, cur, r);
                break;
            default:
                panic(&file, loc, RT_ASSIGN_UNSUPPORTED, "unsupported deref assignment type");
                return ( TQValue){0};
        }
        assign_value(datatypes, &target->val, v);
        return v;
    }

    if (lhs->kind == AST_INDEX) {
        // Evaluate the list
        // Force LIST type lookup to avoid runtime type mismatch errors
        TypedValue target = {0};
        if (lhs->index.target->kind == AST_VAR) {
            target.type = LIST;
            target.val = TQruntime_env_get(lhs->index.target->var, LIST, loc);
        } else {
            target = ast_eval(lhs->index.target);
        }
        
        int idx = ast_eval(lhs->index.index).val.i32;

        ASTNode_t *curr = (ASTNode_t*)target.val.raw;
        for(int i = 0; i < idx && curr; i++) {
            if (curr->kind == AST_SEQ) curr = curr->seq.b;
            else curr = NULL;
        }
        
        if (!curr) {
            panic(&file, loc, RT_UNKNOWN_AST, "List index out of bounds");
            return (TQValue){0};
        }

        // Identify the literal node to update (either the seq element or the tail node)
        ASTNode_t *target_node = (curr->kind == AST_SEQ) ? curr->seq.a : curr;

        // Perform the actual update on the AST node's internal storage
        // This ensures the list contents actually change in the interpreter
        assign_value(datatypes, (TQValue*)&target_node->literal.raw, r);

        return r;
    }


    panic(&file, loc, RT_ASSIGN_TARGET_NOT_VAR, NULL);
    return ( TQValue){0};
}
