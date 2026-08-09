#pragma once

#include "SymbolTable/SymbolTable.hpp"
#include <string>

namespace SA::HIR_SymbolTable {
HIRModule_t *loadOrCreateMod(const char *, HIRNode*);
HIRModule_t *getMode(std::string);
}