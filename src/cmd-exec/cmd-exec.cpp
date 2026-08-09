#include "cmd-exec/cmd-exec.hpp"
#include "HIRGen/HIRGen.hpp"
#include "shared/structs.h"
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

extern std::vector<std::string> ir_out;

static std::string make_object_path(const std::string &input_path) {
  std::string obj_path = input_path;
  if (obj_path.size() >= 3 && obj_path.substr(obj_path.size() - 3) == ".ll") {
    obj_path.resize(obj_path.size() - 3);
  }
  obj_path += ".o";
  return obj_path;
}

static bool compile_ir_to_object(const std::string &ir_path,
                                const std::string &obj_path) {
  std::vector<std::string> args = {"clang++", "-c", "-x", "ir", ir_path,
                                   "-o", obj_path};
  std::vector<char *> argv;
  argv.reserve(args.size());
  for (auto &arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  return run_exec(argv[0], argv.data()) == 0;
}

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
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 1;
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
  opts->bin_output_path = (char *)"SA.bin";
  opts->emit_ir = false;
  opts->ir_output_path = (char *)"out.ll";

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
        syserr("Missing argument for -o\nUsage:  SA [source] [-o bin_path] "
               "[--emit-ir ir_path]");
        return false;
      }
    } else if (strcmp(argv[i], "--emit-ir") == 0) {
      if (i + 1 < argc) {
        opts->emit_ir = true;
        opts->ir_output_path = argv[i + 1];
        i += 2;
      } else {
        syserr("Missing argument for --emit-ir\nUsage:  SA [source] [-o "
               "bin_path] [--emit-ir ir_path]");
        return false;
      }
    } else {
      syserr(logf_msg("Unknown argument: %s\nUsage:  SA [source] [-o bin_path] "
                      "[--emit-ir ir_path]",
                      argv[i]));
      return false;
    }
  }

  if (opts->emit_ir) {
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
    file->filename = (char *)"<stdin>";
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

  HIRGenerator *mgen = new HIRGenerator();
  HIRNode *mast_root = mgen->generate(root);
  delete mgen;

  if (isError)
    return -1;
  // ast_eval_main(root);
  if (codegen(mast_root, opts->emit_ir ? opts->ir_output_path : NULL,
              &ir_text) == EXIT_FAILURE)
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

  std::vector<std::string> object_paths;
  std::string main_obj_path = make_object_path(opts->ir_output_path);
  if (!compile_ir_to_object(opts->ir_output_path, main_obj_path)) {
    if (!opts->emit_ir)
      unlink(opts->ir_output_path);
    return 1;
  }
  object_paths.push_back(main_obj_path);

  for (const auto &import_ir : ir_out) {
    std::string import_obj_path = make_object_path(import_ir);
    if (!compile_ir_to_object(import_ir, import_obj_path)) {
      if (!opts->emit_ir)
        unlink(opts->ir_output_path);
      return 1;
    }
    object_paths.push_back(import_obj_path);
  }

  std::vector<std::string> link_args = {"clang++", "lib/helper/StrHelper.cpp"};
  for (const auto &obj_path : object_paths) {
    link_args.push_back(obj_path);
  }
  link_args.push_back("-Wl,-e,entrypoint");
  link_args.push_back("-no-pie");
  link_args.push_back("-g");
  link_args.push_back("-O0");
  link_args.push_back("-o");
  link_args.push_back(opts->bin_output_path);

  std::vector<char *> link_argv;
  link_argv.reserve(link_args.size());
  for (auto &arg : link_args) {
    link_argv.push_back(arg.data());
  }
  link_argv.push_back(nullptr);

  if (run_exec(link_argv[0], link_argv.data()) != 0) {
    if (!opts->emit_ir)
      unlink(opts->ir_output_path);
    return 1;
  }

  if (!opts->emit_ir)
    unlink(opts->ir_output_path);

  env_clear_all();
  return 0;
}
