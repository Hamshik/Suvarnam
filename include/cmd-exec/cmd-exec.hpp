#pragma once

#ifdef __cplusplus
#include "shared/HIRNode.hpp"
extern "C" {
#endif
#include "shared/structs.h"

#include <limits.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/*------------external fn declaration -----------------------------------*/
void syserr(const char *);
void panic(SA_Location, errc_t, const char *);
char *logf_msg(const char *, ...);

/* Program options structure */
typedef struct {
    const char *input_filename;
    char* bin_output_path;
    bool emit_ir;
    char* ir_output_path;
} Options;
bool parse_arguments(int, char **, Options *);
bool setup_input_file(const Options *, file_t *);
int compile_and_execute(ASTNode_t *, const Options *);
void yyrestart(FILE *);
void semantic_check(ASTNode_t *);
void ast_free(ASTNode_t *);
void env_clear_all();
TypedValue ast_eval_main(ASTNode_t *);

#ifdef __cplusplus
}
int run_exec(const char *, char *const []);
FILE *open_file(const char *, char **);

int codegen(HIRNode *, const char *, char **);
#endif