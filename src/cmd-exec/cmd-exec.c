#include "cmd-exec/cmd-exec.h"
#include "shared/structs.h"
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
bool parse_arguments(int argc, char **argv, Options *opts) {
    // Set defaults
    opts->input_filename = NULL;
    opts->bin_output_path = " TQ.out";
    opts->emit_ir = false;
    opts->ir_output_path = "out.ll";

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
                syserr("Missing argument for -o\nUsage:  TQ [source] [-o bin_path] [--emit-ir ir_path]");
                return false;
            }
        } else if (strcmp(argv[i], "--emit-ir") == 0) {
            if (i + 1 < argc) {
                opts->emit_ir = true;
                opts->ir_output_path = argv[i + 1];
                i += 2;
            } else {
                syserr("Missing argument for --emit-ir\nUsage:  TQ [source] [-o bin_path] [--emit-ir ir_path]");
                return false;
            }
        } else {
            syserr(logf_msg("Unknown argument: %s\nUsage:  TQ [source] [-o bin_path] [--emit-ir ir_path]", argv[i]));
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
bool setup_input_file(const Options *opts, file_t *file) {
    if (!opts->input_filename) {
        file->filename = "<stdin>";
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
int compile_and_execute(ASTNode_t *root, const Options *opts) {
    error_fatal = false; /* collect semantic errors like Rust */
    semantic_check(root);
    error_fatal = true; /* runtime errors should still stop */
    char *ir_text = NULL;
    
    ast_eval_main(root);
    if (codegen(root, opts->emit_ir ? opts->ir_output_path : NULL, &ir_text))
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
        "clang",
        "TQlib/TQstrcmp.c",
        opts->ir_output_path,   // your .ll file
        "-Wl,-e,entrypoint",
        "-no-pie",
        "-g",
        "-O0",
        "-o",
        (char *)opts->bin_output_path,
        NULL
    };

    char* mkdir_argv[] = {
        "mkdir",
        "-p",
        "output"
    };

    if (run_exec(clang_argv[0], clang_argv)) {
        if (!opts->emit_ir)
            unlink(opts->ir_output_path);
        return 1;
    }

    if (!opts->emit_ir)
        unlink(opts->ir_output_path);

    
    if (run_exec(mkdir_argv[0], mkdir_argv)) {
        if (!opts->emit_ir)
            unlink(opts->ir_output_path);
        return 1;
    }

    env_clear_all();
    return 0;
}
