
#include "SymbolTable/SymbolTable.hpp"
#include "shared/HIRNode.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::unique_ptr<HIRMod>> hirMod{};

void die_allocation(const char *what) {
  std::perror(what);
  std::exit(1);
}

namespace SA::HIR_SymbolTable {

HIRMod* getMod(std::string path){
  auto found = hirMod.find(path);
  return found == hirMod.end() ? nullptr : found->second.get();
}

HIRMod *loadOrCreateMod(const char *path, HIRNode* node) {
  HIRMod *existing = getMod(path);
  if (existing) {
    if (existing->state == MOD_LOADING) {
      return nullptr;
    }

    return existing;
  }

  std::unique_ptr<HIRMod> module(new (std::nothrow) HIRMod{});
  if (!module) {
    die_allocation("new");
  }

  module->path = strdup(path);
  if (!module->path) {
    die_allocation("strdup");
  }

  module->state = MOD_LOADING;

  HIRMod *raw = module.get();
  hirMod.emplace(path, std::move(module));

  raw->hirNode = node;

  raw->state = MOD_LOADED;
  raw->parsed = true;
  return raw;
}
} // namespace SA::HIR_SymbolTable