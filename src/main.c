#include "cmd-exec/cmd-exec.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <stdlib.h>

extern FILE *yyin;
extern ASTNode_t* root;
file_t *file;

int yyparse();
Type_t* make_type(DataTypes_t , Type_t*);
void check_err();

int main(int argc, char **argv) {

    file = malloc(sizeof(file_t));
    
    Options opts;
    if (!parse_arguments(argc, argv, &opts)) {
        return EXIT_FAILURE;
    }

    if (!setup_input_file(&opts, file)) {
        return EXIT_FAILURE;
    }
    error_fatal = false;

    yyin = file->source;
    yyrestart(yyin);

    yyparse();
    if (root && !isError) {
        short result = compile_and_execute(root, &opts);
        if (result != 0) {
            return result;
        }
    }
    check_err();


    if (file->source != stdin)
        fclose(file->source);
    if (opts.input_filename && file->filename && file->filename != opts.input_filename)
        free(file->filename);
    return 0;
}
