#include "semantic/TypeChecker.hpp" // New include
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "SymbolTable/SymbolTableInternal.hpp" // For SV_semantic_scope_push/pop, SV_semantic_declare

extern "C" TypedValue ast_eval(ASTNode_t *node) ;
namespace SV::TypeChecker {

} // namespace SV::TypeChecker