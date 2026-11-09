#pragma once

#include "shared/HIRNode.hpp"

#include "shared/structs.h"

#include <stdbool.h>
#include <stdio.h>
#include <string>

class Importer;

/*------------external fn declaration -----------------------------------*/
void syserr(const char *);
void panic(SA::Location, errc_t, const char *);
char *logf_msg(const char *, ...);

/* Program options structure */
struct Options {
  std::string inputFilename;
  std::string binOutputPath = "SA.bin";
  bool emitIr = false;
  std::string irOutputPath = "out.ll";
  bool emitDesugaredSource = false;
  std::string desugaredSourcePath;
};
bool parseArguments(int, char **, Options *);
bool setupInputFile(const Options *, File *);
int compileAndExecute(ASTNode *, const Options *, Importer *importer);
void yyrestart(FILE *);
void semantic_check(ASTNode *);
void ast_free(ASTNode *);

int codegen(HIRNode *, const char *, char **);