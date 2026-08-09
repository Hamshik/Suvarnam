
#include "SymbolTable/SymbolTable.hpp"
#include "shared/HIRNode.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::unique_ptr<HIRModule_t>> hirMod{};

void die_allocation(const char *what) {
  std::perror(what);
  std::exit(1);
}

namespace SA::HIR_SymbolTable {

HIRModule_t* getMod(std::string path){
  auto found = hirMod.find(path);
  return found == hirMod.end() ? nullptr : found->second.get();
}

HIRModule_t *loadOrCreateMod(const char *path, HIRNode* node) {
  HIRModule_t *existing = getMod(path);
  if (existing) {
    if (existing->state == MOD_LOADING) {
      return nullptr;
    }

    return existing;
  }

  std::unique_ptr<HIRModule_t> module(new (std::nothrow) HIRModule_t{});
  if (!module) {
    die_allocation("new");
  }

  module->path = strdup(path);
  if (!module->path) {
    die_allocation("strdup");
  }

  module->state = MOD_LOADING;

  HIRModule_t *raw = module.get();
  hirMod.emplace(path, std::move(module));

  raw->hirNode = node;

  raw->state = MOD_LOADED;
  raw->parsed = true;
  return raw;
}
} // namespace SA::HIR_SymbolTable