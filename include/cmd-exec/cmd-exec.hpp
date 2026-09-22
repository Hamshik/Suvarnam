#pragma once


#include "shared/HIRNode.hpp"

#include "shared/structs.h"

#include <limits.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

class Importer;

/*------------external fn declaration -----------------------------------*/
void syserr(const char *);
void panic(SA_Location, errc_t, const char *);
char *logf_msg(const char *, ...);

/* Program options structure */
typedef struct {
    const char *input_filename;
    char* bin_output_path;
    bool emitIR;
    char* ir_output_path;
} Options;
bool parse_arguments(int, char **, Options *);
bool setup_input_file(const Options *, file_t *);
int compile_and_execute(ASTNode *, const Options *, Importer* import);
void yyrestart(FILE *);
void semantic_check(ASTNode *);
void ast_free(ASTNode *);

int run_exec(const char *, char *const []);
FILE *open_file(const char *, char **);

int codegen(HIRNode *, const char *, char **);