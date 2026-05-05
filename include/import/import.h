#pragma once

#include "shared/structs.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

ASTNode_t* parse_file(FILE *f);
void yyrestart(FILE *input_file);
int yyparse();

#ifdef __cplusplus
}
#endif