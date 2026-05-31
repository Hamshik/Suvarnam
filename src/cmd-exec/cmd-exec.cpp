#include "cmd-exec/cmd-exec.hpp"
#include "MAST-Gen/Mast_gen.hpp"
#include "shared/structs.h"
#include <cstdlib>
#include <stdlib.h>
#include <string.h>

/* Helper function to execute external commands */
int run_exec(const char *prog, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(prog, argv);
        _exit(127);
    }
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    return status;
}

/* Helper function to open and resolve input file */
FILE *open_file(const char *filename, char **resolved_path_out) {
    char resolved[PATH_MAX];
    const char *open_path = filename;
    if (realpath(filename, resolved))
        open_path = resolved;

    FILE *input = fopen(open_path, "rb");
    if (!input) {
        int saved_errno = err_no;
        fprintf(stderr, "Failed to open input: %s\n", filename);
        if (open_path != filename)
            fprintf(stderr, "Resolved path: %s\n", open_path);
        err_no = saved_errno;
        perror("fopen");
        return NULL;
    }
    if (resolved_path_out)
        *resolved_path_out = strdup(open_path);
    return input;
}

/* Parse command-line arguments */
extern "C" bool parse_arguments(int argc, char **argv, Options *opts) {
    // Set defaults
    opts->input_filename = NULL;
    opts->bin_output_path =(char*)"SV.bin";
    opts->emit_ir = false;
    opts->ir_output_path =(char*)"out.ll";

    int i = 1;
    // Parse optional source file
    if (argc > 1 && argv[1][0] != '-') {
        opts->input_filename = argv[1];
        i = 2;
    }

    // Parse flags
    while (i < argc) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                opts->bin_output_path = argv[i + 1];
                i += 2;
            } else {
                syserr("Missing argument for -o\nUsage:  SV [source] [-o bin_path] [--emit-ir ir_path]");
                return false;
            }
        } else if (strcmp(argv[i], "--emit-ir") == 0) {
            if (i + 1 < argc) {
                opts->emit_ir = true;
                opts->ir_output_path = argv[i + 1];
                i += 2;
            } else {
                syserr("Missing argument for --emit-ir\nUsage:  SV [source] [-o bin_path] [--emit-ir ir_path]");
                return false;
            }
        } else {
            syserr(logf_msg("Unknown argument: %s\nUsage:  SV [source] [-o bin_path] [--emit-ir ir_path]", argv[i]));
            return false;
        }
    }

    if(opts->emit_ir) {
        char resolved[PATH_MAX];
        if (realpath(opts->ir_output_path, resolved)) {
            opts->ir_output_path = strdup(resolved);
        }
    }
    return true;
}

/* Set up input file and file_t structure */
extern "C" bool setup_input_file(const Options *opts, file_t *file) {
    if (!opts->input_filename) {
        file->filename = (char*)"<stdin>";
        file->source = stdin;
        return true;
    }

    char *resolved_path = NULL;
    FILE *input = open_file(opts->input_filename, &resolved_path);
    if (!input) {
        return false;
    }
    file->filename = resolved_path ? resolved_path : (char *)opts->input_filename;
    file->source = input;
    return true;
}

/* Compile and execute the AST */
extern "C" int compile_and_execute(ASTNode_t *root, const Options *opts) {
    error_fatal = false; /* collect semantic errors like Rust */
    semantic_check(root);
    error_fatal = true; /* runtime errors should still stop */
    char *ir_text = NULL;

    MASTGenerator *mgen = new MASTGenerator();
    MASTNode *mast_root =  mgen->generate(root);
    delete mgen;
    
    // ast_eval_main(root);
    if (codegen(mast_root, opts->emit_ir ? opts->ir_output_path : NULL, &ir_text) == EXIT_FAILURE)
        return 1;

    ast_free(root);

    FILE *irf = fopen(opts->ir_output_path, "w");
    if (!irf) {
        perror("fopen ll");
        free(ir_text);
        return 1;
    }
    fputs(ir_text ? ir_text : "", irf);
    fclose(irf);
    free(ir_text);

    if (access(opts->ir_output_path, F_OK) != 0) {
        perror("IR file missing before llc");
        return 1;
    }

    char *clang_argv[] = {
       (char*)"clang",
       (char*)"SV_lib/helper/printer.c",
       (char*)"SV_lib/helper/SV_strcmp.c",
        opts->ir_output_path,   // your .ll file
       (char*)"-Wl,-e,entrypoint",
       (char*)"-no-pie",
       (char*)"-g",
       (char*)"-O0",
       (char*)"-o",
        (char *)opts->bin_output_path,
        NULL
    };

    if (run_exec(clang_argv[0], clang_argv)) {
        if (!opts->emit_ir)
            unlink(opts->ir_output_path);
        return 1;
    }

    if (!opts->emit_ir)
        unlink(opts->ir_output_path);

    env_clear_all();
    return 0;
}
