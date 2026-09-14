#pragma once

#include "SymbolTable/SymbolTable.hpp"
#include <string>

namespace SA::HIR_SymbolTable {
HIRMod *loadOrCreateMod(const char *, HIRNode*);
HIRMod *getMod(std::string);
}