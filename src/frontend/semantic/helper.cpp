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
void Semantic::typeError(ASTNode *n, const char *msg) {
  if (n && n->type)
    n->type = new TypeInfo(UNKNOWN, NULL);
  panic(n ? n->loc : (SA_Location){0},
        SEM_BINOP_INVALID, msg ? msg : NULL);
  return;
}

bool Semantic::typesAreEqual(TypeInfo* a, TypeInfo* b) {
    if (a == nullptr && b == nullptr) return true;
    if (!a || !b) return false;

    if (Semantic::isNumeric(a->base) && Semantic::isNumeric(b->base)) return true;
    if (a->base != b->base) return false;

    if (a->base == LIST || a->base == PTR) {
        if (a->size != b->size) return false;
        return Semantic::typesAreEqual(a->inner, b->inner);
    }

    return true;
}

void Semantic::checkErr() {
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

ReturnInfo analyze_returns(ASTNode *n) {
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

bool Semantic::fnAlwaysReturns(ASTNode *body) {
  return analyze_returns(body).always_return;
}