#include "semantic/import.hpp"
#include "frontend/parser/Parser.h"
#include "Scanner.h"
#include "SymbolTable/SymbolTable.hpp"
#include "semantic/semantic.hpp"
#include "shared/nodes.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

std::optional<fs::path>
ImportResolver::resolve(const std::string &import_path,
                        const fs::path &current_file_path) {
  fs::path target(import_path);

  // 1. Absolute Path: If the import path is already absolute, canonicalize it
  if (target.is_absolute()) {
    std::error_code ec;
    auto canonical = fs::canonical(target, ec);
    if (!ec)
      return canonical;
  }

  fs::path current_dir = fs::absolute(current_file_path).parent_path();
  fs::path relative_candidate = current_dir / target;

  std::error_code ec;
  auto canonical_relative = fs::canonical(relative_candidate, ec);
  if (!ec) {
    return canonical_relative;
  }

  for (const auto &include_dir : *include_paths) {
    fs::path candidate = include_dir / target;
    auto canonical_candidate = fs::canonical(candidate, ec);
    if (!ec) {
      return canonical_candidate;
    }
  }

  return std::nullopt;
}

extern ASTNode_t *root;
static bool import_parse_failed = false;

int restart(FILE *f, ASTNode_t* old_root) {
    long saved_pos = ftell(f);
    if (saved_pos < 0) saved_pos = 0;
    rewind(f);

    std::string source;
    char buffer[4096];
    while (size_t nread = fread(buffer, 1, sizeof(buffer), f)) {
        source.append(buffer, nread);
    }

    if (ferror(f)) {
        root = old_root;
        if (saved_pos >= 0) fseek(f, saved_pos, SEEK_SET);
        return -1;
    }

    if (saved_pos >= 0) fseek(f, saved_pos, SEEK_SET);

    // Pass stream directly to Scanner instead of messing with std::cin
    std::istringstream input(source);
    Scanner scanner(input);
    Parser parser(scanner);

    return parser.parse();
}

extern "C" ASTNode_t *parse_file(FILE *f) {
  if (!f)
    return nullptr;

  ASTNode_t *old_root = root; // save current AST
  root = nullptr;             // reset for new parse
  SA_lexer_reset_loc();

  int parse_status = restart(f, old_root);

  if (parse_status == 0) {
    ASTNode_t *new_root = root; // get parsed AST
    root = old_root;            // restore old AST
    return new_root;
  }

  root = old_root; // make sure to restore root on failure too!
  return nullptr;
}

void ensure_semantic(ASTModule_t *m) {
  if (!m || m->semantic_done)
    return;

  semantic_check(m->ast);
  m->semantic_done = true;
}

Type_t* handle_import(ASTNode_t *n) {
  char *path = n->importNode.path;
  bool already_imported = false;
  ASTModule_t *mod = SA_semantic_load_module(path, &already_imported);
  if (!mod) {
    panic(n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);
    import_parse_failed = true;
    return nullptr;
  }

  n->importNode.path = mod->path; // path is already resloved in load_module fn

  if (!mod->parsed) {
    import_parse_failed = true;
    return nullptr;
  }

  if (already_imported)
    return nullptr;

  ensure_semantic(mod);

  check_expr(mod->ast);

  return nullptr;
}