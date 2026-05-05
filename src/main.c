#include "cmd-exec/cmd-exec.h"
#include <stdlib.h>

extern FILE *yyin;
file_t file = {0};
extern ASTNode_t* root;

int yyparse();

int main(int argc, char **argv) {
    Options opts;
    if (!parse_arguments(argc, argv, &opts)) {
        return EXIT_FAILURE;
    }

    if (!setup_input_file(&opts, &file)) {
        return EXIT_FAILURE;
    }

    yyin = file.source;
    yyrestart(yyin);

    yyparse();
    if (root) {
        int result = compile_and_execute(root, &opts);
        if (result != 0) {
            return result;
        }
    } else {
        fprintf(stderr, "Parsing failed with errors.\n");
        return EXIT_FAILURE;
    }

    if (file.source != stdin)
        fclose(file.source);
    if (opts.input_filename && file.filename && file.filename != opts.input_filename)
        free(file.filename);
    return 0;
}
