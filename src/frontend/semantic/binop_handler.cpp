
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "semantic/semantic.hpp"
#include <cstddef>

SA::Type* Semantic::binop(ASTNode *n, SA::Type* type) {
  // Use &type to allow inference to flow into children
  SA::Type* lt = checkExpr(n->bin.left, type);
  SA::Type* rt = checkExpr(n->bin.right, type);

  // 1. Numeric Literal Inference (Improved)
  if (n->bin.left->kind == AST_NUM && (!n->bin.left->type || n->bin.left->type->base == UNKNOWN)) {
      if (isNumeric(rt->base)) { n->bin.left->type = rt; lt = rt; }
  } 
  else if (n->bin.right->kind == AST_NUM && n->bin.right->type->base == UNKNOWN) {
      if (isNumeric(lt->base)) { n->bin.right->type = lt; rt = lt; }
  }

  // 2. Disallow Pointer Arithmetic (as per your requirement)
  if (lt->base == PTR || rt->base == PTR) {
    panic(n->loc, SEM_NUMOP_NEEDS_NUM, "pointer arithmetic not supported");
  }

  // 3. String Concatenation & multiplication
  if (lt->base == STRINGS || rt->base == STRINGS) {
    
    bool is_valid_mul = (n->bin.op == OP_MUL) && 
                        ((lt->base == STRINGS && isNumeric(rt->base)) || 
                         (isNumeric(lt->base) && rt->base == STRINGS));
    bool is_valid_add = (n->bin.op == OP_ADD) && (lt->base == STRINGS && rt->base == STRINGS);

    if (!is_valid_mul && !is_valid_add) {
      panic(n->loc, SEM_STRING_OP_INVALID, NULL);
    }

    // Re-use a global type pointer if possible to save memory
    n->type = new SA::Type(STRINGS, NULL); 
    return n->type;
  }

  // 4. Comparison Operators
  switch (n->bin.op) {
    case OP_LT: case OP_LE: case OP_GT: case OP_GE:
    case OP_EQ: case OP_NEQ:
      if (!isNumeric(lt->base) || !isNumeric(rt->base)) {
          // If they aren't numeric, allow EQ/NEQ only if they are the exact same type
          if ((n->bin.op == OP_EQ || n->bin.op == OP_NEQ) && typesAreEqual(lt, rt)) {
              // Valid (e.g., comparing two lists or strings)
          } else {
              panic(n->loc, SEM_CMP_NEEDS_NUM, NULL);
          }
      }
      n->type = new SA::Type(BOOL, NULL);
      return n->type;

    case OP_AND: case OP_OR:
      if (lt->base != BOOL || rt->base != BOOL)
        panic(n->loc, SEM_LOGIC_NEEDS_BOOL, NULL);
      n->type = new SA::Type(BOOL, NULL);
      return n->type;

    default:
      // 5. Arithmetic & Bitwise
      if (!isNumeric(lt->base) || !isNumeric(rt->base))
        panic(n->loc, SEM_NUMOP_NEEDS_NUM, NULL);

      if (n->bin.op == OP_LSHIFT || n->bin.op == OP_RSHIFT ||
          n->bin.op == OP_BITAND || n->bin.op == OP_BITOR || n->bin.op == OP_BITXOR) {
        if (!isInt(lt->base) || !isInt(rt->base)) {
          panic(n->loc, SEM_NUMOP_NEEDS_NUM, "bitwise ops require integer types");
        }
      }

      // Promote returns the dominant base type (e.g., f64 > i32)
      DataTypes_t promted_t = promote(lt->base, rt->base);
      n->type = n->type->base == promted_t ? n->type : new SA::Type(promted_t, NULL);
      return n->type;
  }
}
