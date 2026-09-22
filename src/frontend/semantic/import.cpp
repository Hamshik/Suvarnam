#include "semantic/import.hpp"
#include "Parser.h"
#include "SymbolTable/SymbolTable.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/nodes.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

Importer::Importer(FILE *source, CompilerContext *context)
    : ctx(context), f(source) {
  content.reset(getFileContent(f));
  ctx = new CompilerContext(*content);
}

std::optional<fs::path> Importer::resolve(const std::string &import_path,
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

extern ASTNode *root;
static bool import_parse_failed = false;

std::istringstream *Importer::getFileContent(FILE *f) {
  long saved_pos = ftell(f);
  if (saved_pos < 0)
    saved_pos = 0;
  rewind(f);

  std::string source;
  char buffer[4096];
  while (size_t nread = fread(buffer, 1, sizeof(buffer), f)) {
    source.append(buffer, nread);
  }

  if (ferror(f)) {
    syserr("Fail to read the file");
  }

  if (saved_pos >= 0)
    fseek(f, saved_pos, SEEK_SET);

  std::istringstream *content = new std::istringstream(source);
  return content;
}

ASTNode *Importer::parseFile() {
  if (!f)
    return nullptr;

  ASTNode *old_root = root; // save current AST
  root = nullptr;           // reset for new parse

  if (ctx->parser->parse() == 0 || !isError) {
    ASTNode *new_root = root; // get parsed AST
    root = old_root;          // restore old AST
    return new_root;
  }

  root = old_root; // make sure to restore root on failure too!
  return nullptr;
}

void Importer::ensureSemantic(ASTMod *m) {
  if (!m || m->semantic_done)
    return;

  m->semantic_done = true;
}

TypeInfo *Importer::handleImport(ASTNode *n) {
  char *path = n->importNode.path;
  bool already_imported = false;
  ASTMod *mod = ctx->sym->loadMod(path, file->filename, already_imported);
  if (!mod) {
    panic(n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);
    import_parse_failed = true;
    return nullptr;
  }

  n->importNode.path = mod->path; // path is already resloved in loadMod fn

  if (!mod->parsed) {
    import_parse_failed = true;
    return nullptr;
  }

  if (already_imported)
    return nullptr;

  ensureSemantic(mod);

  return nullptr;
}