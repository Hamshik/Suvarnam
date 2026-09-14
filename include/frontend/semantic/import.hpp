#pragma once

#include "SymbolTable/SymbolTable.hpp"
#include "shared/nodes.h"
#include "shared/structs.h"
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

class Importer {
private:
  std::vector<fs::path> *include_paths; // e.g., stdlib paths, -I flags
  CompilerContext* ctx;
  std::unique_ptr<std::istringstream> content;
  bool isFreshCtx = true;
  FILE* f;

public:
  explicit Importer(FILE* f, CompilerContext* ctx = nullptr);
  ~Importer(){
    if(isFreshCtx) delete ctx;
  }

  CompilerContext* getCtx(){ return ctx; }

  // Main resolution logic
  std::optional<fs::path> resolve(const std::string &import_path,
                                  const fs::path &current_file_path);
  void updatePaths(std::vector<fs::path> *f) { include_paths = f; }
  void ensureSemantic(ASTMod *);
  TypeInfo *handleImport(ASTNode *);

  ASTNode *parseFile();

  std::istringstream* getFileContent(FILE *f);
};