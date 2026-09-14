#include "cmd-exec/cmd-exec.hpp"
#include "parser/parser_helpers.h"
#include "semantic/import.hpp"
#include "semantic/semantic.hpp"
#include "shared/structs.h"
#include <stdlib.h>

ASTNode* root{};
file_t *file{};

int main(int argc, char **argv) {

    file = new file_t;
    
    Options opts;
    if (!parse_arguments(argc, argv, &opts)) {
        return EXIT_FAILURE;
    }

    if (!setup_input_file(&opts, file)) {
        return EXIT_FAILURE;
    }
    error_fatal = false;

    int status = 0;
    auto importer = new Importer(file->source);
    importer->parseFile();
    
    if (root && !isError)
        status = compile_and_execute(root, &opts, importer);
    
    Semantic::checkErr();
    if (file->source != stdin)
        fclose(file->source);
    if (opts.input_filename && file->filename && file->filename != opts.input_filename)
        free(file->filename);
    return status;
}
