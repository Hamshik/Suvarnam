#pragma once

#include "SymbolTable/SymbolTable.hpp"
#include "shared/nodes.h"
#include <filesystem>
#include <optional>
#include <vector>

namespace fs = std::filesystem;
class SemanticSymTable;
class Importer {
private:
  std::vector<fs::path> *include_paths; // e.g., stdlib paths, -I flags
  SemanticSymTable* sym; 

public:
  explicit Importer(SemanticSymTable* sym): sym(sym) {}
  // Main resolution logic
  std::optional<fs::path> resolve(const std::string &import_path,
                                  const fs::path &current_file_path);
  void updatePaths(std::vector<fs::path> *f) { include_paths = f; }
  void ensureSemantic(ASTModule_t *);
  TypeInfo *handleImport(ASTNode *);

  static ASTNode *parseFile(FILE *);
  static int restart(FILE *, ASTNode *);
};