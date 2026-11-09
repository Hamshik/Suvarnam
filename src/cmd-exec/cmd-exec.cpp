#include "cmd-exec/cmd-exec.hpp"
#include "HIRGen/HIRGen.hpp"
#include "linker.hpp"
#include "semantic/import.hpp"
#include "semantic/semantic.hpp"
#include "shared/structs.h"
#include "utils/colors.h"
#include "utils/error_handler/error.h"

#include <print>
#include <string>
#include <vector>

extern std::vector<std::string> irOut;
int codegen(HIRNode *, const char *, char **, bool);
SemanticSymTable *semanticSymbols;
SemanticSymTable *sym;

int compileAndExecute(ASTNode *root, const Options *options,
                      Importer *importer) {
  error_fatal = false;

  SA::CompilerContext *context = importer->getCtx();
  context->semantic.main(root);

  semanticSymbols = &context->sym;
  sym = semanticSymbols;
  delete importer;

  error_fatal = true;
  char *irText = nullptr;

  Semantic::checkErr();

  HIRGenerator *hirGenerator = new HIRGenerator(context);
  HIRNode *mastRoot = hirGenerator->generateModule(root);
  delete hirGenerator;

  if (isError)
    return -1;

  irOut.clear();
  if (options->emitDesugaredSource) {
    const bool emitted =
        writeDesugaredSource(mastRoot, options->desugaredSourcePath);
    ast_free(root);
    if (emitted) {
      std::println(SA_GREEN "SUCESS: Desuggering the source code" SA_RESET);
      return 0;
    }
    syserr("ERROR: Desugering the source code");
    return 1;
  }

  const char *mainIrPath =
      (options->emitIr && !options->irOutputPath.empty())
          ? options->irOutputPath.c_str()
          : "/tmp/temp_suvarnam_main.ll";

  if (codegen(mastRoot, mainIrPath, &irText, true) == EXIT_FAILURE) {
    printf("Codegen is exited with 1");
    return 1;
  }

  ast_free(root);

  if (options->emitIr && !options->irOutputPath.empty()) {
    FILE *irFile = fopen(options->irOutputPath.c_str(), "w");
    if (irFile) {
      fputs(irText ? irText : "", irFile);
      fclose(irFile);
    }
  }
  free(irText);

  return compileAndLink(mainIrPath, options->binOutputPath, options->emitIr);
}
