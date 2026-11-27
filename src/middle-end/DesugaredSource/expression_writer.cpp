#include "DesugaredSourceWriter.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"

#include <ostream>

namespace DesugaredSource {

static bool getBuiltinVarargFixedCount(const char *name, size_t &fixedCount) {
  BuiltinFunction *builtin = BuiltinRegistry::instance().lookup(name);
  if (!builtin)
    return false;

  for (const SA::Param *param : builtin->param_types) {
    if (!param || !param->type)
      return false;
    if (param->type->base == UNKNOWN)
      return true;
    ++fixedCount;
  }
  return false;
}

void Writer::writeName(const char *name) {
  if (!name)
    return;
  for (const char *p = name; *p; ++p)
    out_ << (static_cast<unsigned char>(*p) >= 32 ? *p : '_');
}

bool Writer::writeExpression(const HIRNode *node) {
  if (!node)
    return false;
  switch (node->kind) {
  case AST_NUM:
    return writeNumber(node);
  case AST_STR:
    return writeEscaped(out_, node->val.chars, '"');
  case AST_CHAR:
    return writeEscaped(out_, node->val.chars, '\'');
  case AST_BOOL:
    text(node->val.bval ? "true" : "false");
    return true;
  case AST_VAR:
    if (!node->name)
      return false;
    if(node->isglobal)
      character('@');
    writeName(node->name);
    return true;
  case AST_BINOP: {
    const char *op = operatorName(node->binary.op);
    if (!op)
      return false;
    character('(');
    if (!writeExpression(node->binary.left))
      return false;
    character(' ');
    text(op);
    character(' ');
    if (!writeExpression(node->binary.right))
      return false;
    character(')');
    return true;
  }
  case AST_UNOP: {
    const char *op = operatorName(node->binary.op);
    if (!op)
      return false;
    if (node->binary.op == OP_INC || node->binary.op == OP_DEC) {
      if (!writeExpression(node->binary.left))
        return false;
      text(op);
      return true;
    }
    text(op);
    return writeExpression(node->binary.left);
  }
  case AST_CALL:
    if (!node->call.targetFb || !node->call.args)
      return false;
    writeName(node->call.targetFb);
    character('(');
    {
      size_t fixedParamCount = 0;
      const bool isBuiltinVararg =
          getBuiltinVarargFixedCount(node->call.targetFb, fixedParamCount);
      bool firstArg = true;
      auto writeArg = [&](const HIRNode *arg) {
        if (!firstArg)
          text(", ");
        firstArg = false;
        return writeExpression(arg);
      };

      for (size_t i = 0; i < node->call.args->size(); ++i) {
        const HIRNode *arg = (*node->call.args)[i];
        if (isBuiltinVararg && i == fixedParamCount && arg &&
            arg->kind == AST_LIST && arg->element.elements) {
          for (const HIRNode *packedArg : *arg->element.elements) {
            if (!writeArg(packedArg))
              return false;
          }
        } else if (!writeArg(arg)) {
          return false;
        }
      }
    }
    character(')');
    return true;
  case AST_LIST:
    if (!node->element.elements)
      return false;
    character('[');
    for (size_t i = 0; i < node->element.elements->size(); ++i) {
      if (i)
        text(", ");
      if (!writeExpression((*node->element.elements)[i]))
        return false;
    }
    character(']');
    return true;
  case AST_INDEX:
    if (!writeExpression(node->index.target) || !node->index.idx)
      return false;
    for (const HIRNode *index : *node->index.idx) {
      character('[');
      if (!writeExpression(index))
        return false;
      character(']');
    }
    return true;
  case AST_MEMBER:
    if (!writeExpression(node->memberAccess.target))
      return false;
    character('.');
    writeName(node->memberAccess.name.c_str());
    return true;
  case AST_STRUCT_INIT:
    if (!node->structInit.name || !node->structInit.fields)
      return false;
    writeName(node->structInit.name);
    text(" { ");
    for (size_t i = 0; i < node->structInit.fields->size(); ++i) {
      if (i)
        text(", ");
      const Fields *field = (*node->structInit.fields)[i];
      if (!field || field->nodePtrKind != Fields::NodePtrKind::HIR ||
          !field->nodePtr.hirNode)
        return false;
      writeName(field->name.c_str());
      text(": ");
      if (!writeExpression(field->nodePtr.hirNode))
        return false;
    }
    text(" }");
    return true;
  case AST_RANGE:
    if (!writeExpression(node->range.start))
      return false;
    text("..");
    if (!writeExpression(node->range.end))
      return false;
    if (node->range.step) {
      text("..");
      if (!writeExpression(node->range.step))
        return false;
    }
    return true;
  default:
    return false;
  }
}

} // namespace DesugaredSource
