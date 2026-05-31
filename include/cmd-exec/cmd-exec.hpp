#pragma once

#ifdef __cplusplus
#include "shared/M_node.hpp"
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
void syserr(const char *context);
void panic( SV_Location loc, errc_t code, const char *detail);
char* logf_msg(const char *fmt, ...);

/* Program options structure */
typedef struct {
    const char *input_filename;
    char* bin_output_path;
    bool emit_ir;
    char* ir_output_path;
} Options;
bool parse_arguments(int argc, char **argv, Options *opts);
bool setup_input_file(const Options *opts, file_t *file);
int compile_and_execute(ASTNode_t *root, const Options *opts);
void yyrestart(FILE *input_file);
void semantic_check(ASTNode_t *root);
void ast_free(ASTNode_t *n);
void env_clear_all();
TypedValue ast_eval_main(ASTNode_t *root);

#ifdef __cplusplus
}
int run_exec(const char *prog, char *const argv[]);
FILE *open_file(const char *filename, char **resolved_path_out);

int codegen(MASTNode *root, const char *ll_path, char **ir_out);
#endif