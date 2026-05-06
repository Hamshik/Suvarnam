#include "shared/enums.h"
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

// We cannot use assign_value directly because literal.raw is a char* pointer.
// Overwriting it with binary data (like an int32) destroys the pointer value,
// leading to segfaults when handle_num() tries to dereference it.
// Instead, we update the string representation so handle_num can re-parse it.

static void update_val(TQValue r, ASTNode_t *dst){
    if (!dst) return;
    TQLocation loc = dst->loc;
    DataTypes_t datatypes = dst->type->base;
    char buf[128];
    char *new_raw = NULL;
    switch (datatypes) {
        case I8:   sprintf(buf, "%d", (int)r.i8); break;
        case I16:  sprintf(buf, "%d", (int)r.i16); break;
        case I32:  sprintf(buf, "%d", r.i32); break;
        case I64:  sprintf(buf, "%ld", r.i64); break;
        case U8:   sprintf(buf, "%u", (unsigned int)r.u8); break;
        case U16:  sprintf(buf, "%u", (unsigned int)r.u16); break;
        case U32:  sprintf(buf, "%u", r.u32); break;
        case U64:  sprintf(buf, "%llu", (unsigned long long)r.u64); break;
        case F32:  sprintf(buf, "%g", (double)r.f32); break;
        case F64:  sprintf(buf, "%g", r.f64); break;
        case STRINGS: new_raw = strdup(r.str); break;
        case CHARACTER: buf[0] = (char)(*r.chars); buf[1] = '\0'; break;
        case BOOL: strcpy(buf, r.bval ? "true" : "false"); break;
        default:
            panic(&file, loc, RT_ASSIGN_UNSUPPORTED, "Update for this type in list index not supported");
            return;
    }

    if (!new_raw) new_raw = strdup(buf);
    if (dst->literal.raw) free(dst->literal.raw); 
    // Update the literal. Next time handle_num is called, it parses this new string.
    dst->literal.raw = new_raw;
}

TQValue eval_assign(ASTNode_t *lhs, ASTNode_t *rhs, OP_kind_t op, Type_t* type , TQLocation loc) {
    TypedValue rt0 = ast_eval(rhs);
    TypedValue rt = TQcast_typed(rt0, type->base);
    TQValue r = rt.val;
    TQValue v = {0};

    if (!lhs) {
        panic(&file, loc, RT_ASSIGN_TARGET_NOT_VAR, NULL);
        return ( TQValue){0};
    }

    /* Assignment to variable */
    if (lhs->kind == AST_VAR) {
        if (op == OP_ASSIGN) {
            set_var(lhs->var, &r, type);
            return r;
        }

        TQValue cur = getvar(lhs->var, type, loc);
        OP_kind_t operation = get_assign_op(op);
        switch (type->base) {
            case I8: case I16: case I32: case I128:
            case U8: case U16: case U32: case U64: case U128:
            case F32: case F64: case F128:
            case UF32: case UF64: case UF128:
                v = TQeval_binop_numeric(operation, type->base, cur, r);
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
        set_var(lhs->var, &v, type);
        return v;
    }

    // TypedValue right_val = ast_eval(rhs);

    /* Assignment through dereference: *p = rhs */
    if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
        TypedValue pv = ast_eval(lhs->unop.operand);
        if (pv.type->base != PTR || pv.val.ptr.name == NULL) {
            panic(&file, loc, RT_DANGLING_PTR, NULL);
            return ( TQValue){0};
        }
        TypedValue *target = getvar_ref_at(pv.val.ptr.frame_id, pv.val.ptr.name, loc);
        if (!target) return ( TQValue){0};
        if (target->type != type) {
            panic(&file, loc, RT_VAR_TYPE_MISMATCH, pv.val.ptr.name);
            return ( TQValue){0};
        }

        if (op == OP_ASSIGN) {
            assign_value(type->base, &target->val, r);
            return r;
        }

       TQValue cur = target->val;
        OP_kind_t operation = get_assign_op(op);
        switch (type->base) {
            case I8: case I16: case I32: case I128:
            case U8: case U16: case U32: case U64: case U128:
            case F32: case F64: case F128:
            case UF32: case UF64: case UF128:
                v = TQeval_binop_numeric(operation, type->base, cur, r);
                break;
            default:
                panic(&file, loc, RT_ASSIGN_UNSUPPORTED, "unsupported deref assignment type");
                return ( TQValue){0};
        }
        assign_value(type->base, &target->val, v);
        return v;
    }

    if (lhs->kind == AST_INDEX) {
        TypedValue target_val = ast_eval(lhs->index.target);
        int idx = ast_eval(lhs->index.index).val.i32;

        // The raw pointer points to the head of the AST_SEQ chain
        ASTNode_t *curr = (ASTNode_t*)target_val.val.raw;
        
        // Traverse the sequence to find the correct element
        for(int i = 0; i < idx && curr; i++) {
            if (curr->kind == AST_SEQ) curr = curr->seq.b;
            else curr = NULL;
        }
        
        if (!curr) {
            panic(&file, loc, RT_UNKNOWN_AST, "List index out of bounds");
            return (TQValue){0};
        }

        ASTNode_t *target_node = (curr->kind == AST_SEQ) ? curr->seq.a : curr;

        // RECURSIVE CHECK:
        // If the element we found is another LIST, we don't 'update_val' (string conversion)
        // Instead, we replace the pointer.
        if (target_node->kind == AST_LIST) {
            // Rust/Python list behavior: matrix[0] = [1, 2]
            // Update the pointer in the sequence node
            target_node->list.elements = (ASTNode_t*)r.raw; 
        } else {
            // Standard behavior: list[0] = 5
            update_val(r, target_node);
        }

        return r;
    }



    panic(&file, loc, RT_ASSIGN_TARGET_NOT_VAR, NULL);
    return (TQValue){0};
}
