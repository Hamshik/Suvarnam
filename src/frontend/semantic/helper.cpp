#include <stdio.h>
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/colors.h"
#include "utils/error_handler/error.h"

extern file_t* file;

typedef struct {
  bool always_return;
  bool always_fallthrough;
} ReturnInfo;

/* Helpers */
void type_error(ASTNode_t *n, const char *msg) {
  if (n && n->type)
    n->type = make_type(UNKNOWN, NULL);
  panic(n ? n->loc : (SA_Location){0},
        SEM_BINOP_INVALID, msg ? msg : NULL);
  return;
}


bool types_are_equal(Type_t* a, Type_t* b) {
    // 1. If both are null, they are equal (base case)
    if (a == nullptr && b == nullptr) return true;
    // If one is null and the other is not, they are not equal
    if (!a || !b) return false;

    if (is_numeric(a->base) && is_numeric(b->base)) return true;
    if (a->base != b->base) return false;
    
    // 3. For Lists and Pointers, check sizes and inner types
    if (a->base == LIST || a->base == PTR) {
        if (a->size != b->size) return false;
        return types_are_equal(a->inner, b->inner);
    }

    return true;
}


extern "C" void check_err() {
  if (isError && isWarning) {
    fprintf(stderr, SA_BOLD SA_RED "ERROR: " SA_RESET);
    fprintf(stderr,
            SA_UNDERLINE SA_MAGENTA
            "Compilation failed with %zu error(s) and %zu warning(s)\n" SA_RESET,
            err_no, warn_no);
    exit(EXIT_FAILURE);
  } else if (isError) {
    fprintf(stderr, SA_BOLD SA_RED "ERROR: " SA_RESET);
    fprintf(stderr,
            SA_UNDERLINE SA_MAGENTA "Compilation failed with %zu error(s)\n" SA_RESET,
            err_no);
    exit(EXIT_FAILURE);
  } else if (isWarning) {
    fprintf(stderr, SA_BOLD SA_YELLOW "WARNING: " SA_RESET);
    fprintf(stderr,
            SA_UNDERLINE SA_MAGENTA
            "Compilation succeeded with %zu warning(s)\n" SA_RESET,
            warn_no);
  } 
}

ReturnInfo analyze_returns(ASTNode_t *n) {
  if (!n) return (ReturnInfo){false, true};

  switch (n->kind) {
  case AST_RETURN:
    return (ReturnInfo){true, false};

  case AST_SEQ: {
    ReturnInfo left = analyze_returns(n->seq.a);
    if (left.always_return)
      return (ReturnInfo){true, false};
    ReturnInfo right = analyze_returns(n->seq.b);
    return (ReturnInfo){
      left.always_fallthrough && right.always_return,
      left.always_fallthrough && right.always_fallthrough
    };
  }

  case AST_IF: {
    ReturnInfo then_info = analyze_returns(n->ifnode.then_branch);
    if(*n->ifnode.cond->literal.raw == 't')
      return (ReturnInfo){true, true};
    if (!n->ifnode.else_branch)
      return (ReturnInfo){false, true};
    ReturnInfo else_info = analyze_returns(n->ifnode.else_branch);
    return (ReturnInfo){
      *n->ifnode.cond->literal.raw == 'f' ?
      else_info.always_return : then_info.always_return && else_info.always_return,
      then_info.always_fallthrough && else_info.always_fallthrough
    };
  }

  case AST_WHILE:
  case AST_FOR:
    return (ReturnInfo){false, true};

  case AST_BREAK:
  case AST_CONTINUE:
    return (ReturnInfo){false, false};

  case AST_BLOCK:
    return analyze_returns(n->block.block);

  default:
    return (ReturnInfo){false, true};
  }
}

bool fn_always_returns(ASTNode_t *body) {
  return analyze_returns(body).always_return;
}