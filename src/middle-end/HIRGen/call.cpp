#include "shared/HIRNode.hpp"
#include "shared/nodes.h"
#include "SymbolTable/SymbolTable.hpp"
#include "HIRGen/HIRGen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"

void panic(SA::Location loc, errc_t code, const char *detail);
unsigned __int128 SA_parse_u128(const char *str, int *ok);
__int128 SA_parse_i128(const char *str, int *ok);


bool isVariadicBuiltin(const char *name) {
  if (!name) {
    return false;
  }

  BuiltinFunction *builtin = BuiltinRegistry::instance().lookup(name);
  if (!builtin) {
    return false;
  }

  for (auto *param : builtin->param_types) {
    if (param && param->type->base == UNKNOWN) {
      return true;
    }
  }

  return false;
}

size_t countFixedBuiltinParams(const char *name) {
  if (!name) {
    return 0;
  }

  BuiltinFunction *builtin = BuiltinRegistry::instance().lookup(name);
  if (!builtin) {
    return 0;
  }

  size_t fixedParams = 0;
  for (auto *param : builtin->param_types) {
    if (!param || param->type->base == UNKNOWN) {
      break;
    }
    ++fixedParams;
  }

  return fixedParams;
}

struct VarargCallState {
  HIRNode *callNode;
  bool isBuiltinVararg;
  size_t fixedParamCount;
  bool hasUserVarargs;
  size_t fixedUserParamCount;
  FnSymbol *fnSymbol;
  std::vector<SA::Type *> varargTypes;
  std::vector<std::vector<HIRNode *>> varargGroups;
  std::vector<HIRNode *> *packedArgs = nullptr;
  size_t currentGroup = 0;
  size_t argIndex = 0;
};

// Collect inner types for variadic user parameters (in declaration order)
std::vector<SA::Type *> collectVarargTypes(FnSymbol *fnSymbol,
                                           size_t fixedParamCount) {
  std::vector<SA::Type *> types;
  if (!fnSymbol || !fnSymbol->params) return types;
  for (int i = fixedParamCount; i < fnSymbol->paramCount; ++i) {
    if (fnSymbol->params[i].is_variadic) {
      SA::Type *innerType = fnSymbol->params[i].type
                                ? fnSymbol->params[i].type->inner
                                : nullptr;
      types.push_back(innerType);
    }
  }
  return types;
}

// Distribute a lowered argument into either builtin packed vector, one of the
// user variadic groups, or append directly to the call args when not variadic.
void distributeArg(HIRNode *loweredArg, VarargCallState &state) {
  if (!loweredArg) return;
  const size_t argIndex = state.argIndex++;

  if (state.isBuiltinVararg && argIndex >= state.fixedParamCount) {
    if (!state.packedArgs)
      state.packedArgs = new std::vector<HIRNode *>();
    state.packedArgs->push_back(loweredArg);
    return;
  }

  if (state.hasUserVarargs && argIndex >= state.fixedUserParamCount) {
    if (state.varargTypes.empty()) {
      // fallback to packed
      if (!state.packedArgs)
        state.packedArgs = new std::vector<HIRNode *>();
      state.packedArgs->push_back(loweredArg);
      return;
    }

    size_t group = state.currentGroup;
    while (group < state.varargTypes.size() &&
           !(state.varargTypes[group] == nullptr ||
             (loweredArg->type &&
              state.varargTypes[group]->base == loweredArg->type->base))) {
      ++group;
    }
    if (group >= state.varargGroups.size()) {
      state.varargGroups.back().push_back(loweredArg);
      state.currentGroup = state.varargGroups.size() - 1;
    } else {
      state.varargGroups[group].push_back(loweredArg);
      state.currentGroup = group;
    }
    return;
  }

  // Non-variadic: append to call args
  if (state.callNode->call.args)
    state.callNode->call.args->push_back(loweredArg);
}

HIRNode *makeVarargList(std::vector<HIRNode *> *elements,
                        SA::Type *innerType) {
  HIRNode *varargList = new HIRNode(ASTKind::AST_LIST);
  varargList->element.elements = elements;
  varargList->type = new SA::Type(LIST, nullptr);
  varargList->type->inner = innerType;
  varargList->type->size = elements->size();
  return varargList;
}

void appendVarargCount(HIRGenerator &generator, HIRNode *callNode,
                       size_t count) {
  SA::Value rawCount = {0};
  rawCount.i64 = static_cast<int64_t>(count);
  SA::Type *countType = new SA::Type(I64, nullptr);
  HIRNode *countArg =
      generator.createLiteral(rawCount, countType, ASTKind::AST_NUM);
  callNode->call.args->push_back(countArg);
}

void appendVarargGroup(HIRGenerator &generator, const VarargCallState &state,
                       size_t groupIndex) {
  const auto &group = state.varargGroups[groupIndex];
  auto *elements = new std::vector<HIRNode *>(group.begin(), group.end());
  state.callNode->call.args->push_back(
      makeVarargList(elements, state.varargTypes[groupIndex]));
  appendVarargCount(generator, state.callNode, elements->size());
}

SA::Type *packedVarargType(const VarargCallState &state) {
  if (!state.fnSymbol || !state.fnSymbol->params ||
      state.fixedUserParamCount >= state.fnSymbol->paramCount) {
    return nullptr;
  }

  SA::Type *paramType = state.fnSymbol->params[state.fixedUserParamCount].type;
  return paramType ? paramType->inner : nullptr;
}

void appendPackedVarargs(HIRGenerator &generator,
                         const VarargCallState &state) {
  state.callNode->call.args->push_back(
      makeVarargList(state.packedArgs, packedVarargType(state)));
  appendVarargCount(generator, state.callNode, state.packedArgs->size());
}

void appendBuiltinVarargs(const VarargCallState &state) {
  state.callNode->call.args->push_back(makeVarargList(state.packedArgs, nullptr));
}

// Emit grouped user varargs or packed builtin varargs with their hidden lengths.
void appendUserVarargs(HIRGenerator &generator, const VarargCallState &state) {
  if (!state.callNode) return;

  if (!state.varargGroups.empty()) {
    for (size_t i = 0; i < state.varargGroups.size(); ++i) {
      appendVarargGroup(generator, state, i);
    }
    return;
  }

  if (state.packedArgs)
    appendPackedVarargs(generator, state);
}

VarargCallState makeVarargState(HIRNode *callNode, const char *name,
                                FnSymbol *fnSymbol) {
  const bool isBuiltinVararg = isVariadicBuiltin(name);
  const size_t fixedParamCount =
      isBuiltinVararg ? countFixedBuiltinParams(name) : 0;

  bool hasUserVarargs = false;
  size_t fixedUserParamCount = 0;
  if (fnSymbol && fnSymbol->params) {
    for (int i = 0; i < fnSymbol->paramCount; ++i) {
      if (fnSymbol->params[i].is_variadic) {
        hasUserVarargs = true;
        break;
      }
      ++fixedUserParamCount;
    }
  }

  VarargCallState state{
      .callNode = callNode,
      .isBuiltinVararg = isBuiltinVararg,
      .fixedParamCount = fixedParamCount,
      .hasUserVarargs = hasUserVarargs,
      .fixedUserParamCount = fixedUserParamCount,
      .fnSymbol = fnSymbol,
      .varargTypes = collectVarargTypes(fnSymbol, fixedUserParamCount)};
  state.varargGroups.resize(state.varargTypes.size());
  return state;
}

HIRNode *makeCallNode(ASTNode *node) {
  HIRNode *callNode = new HIRNode(ASTKind::AST_CALL);
  callNode->name = strdup(node->call.name);
  callNode->call.args = new std::vector<HIRNode *>();
  return callNode;
}

void lowerArgs(HIRGenerator &generator, ASTNode *node,
               VarargCallState &state) {
  for (ASTNode *current = node->call.args; current;) {
    ASTNode *argExpr = (current->kind == AST_SEQ) ? current->seq.a : current;
    HIRNode *loweredArg = generator.generate(argExpr);
    distributeArg(loweredArg, state);
    current = (current->kind == AST_SEQ) ? current->seq.b : nullptr;
  }
}

void appendCallVarargs(HIRGenerator &generator, const VarargCallState &state) {
  if (state.hasUserVarargs) {
    appendUserVarargs(generator, state);
  } else if (state.isBuiltinVararg && state.packedArgs &&
             !state.packedArgs->empty()) {
    appendBuiltinVarargs(state);
  }
}

HIRNode *HIRGenerator::emitCall(ASTNode *node) {
  HIRNode *callNode = makeCallNode(node);
  FnSymbol *fnSymbol = ctx->sym.fnFind(node->call.name);
  VarargCallState state =
      makeVarargState(callNode, node->call.name, fnSymbol);
  lowerArgs(*this, node, state);
  appendCallVarargs(*this, state);

  callNode->type = node->type;
  callNode->loc = node->loc;
  return callNode;
}