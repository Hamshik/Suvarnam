#include "DesugaredSourceWriter.hpp"

#include <cstring>
#include <ostream>

namespace DesugaredSource {

bool Writer::write(const HIRNode *node) { return writeStatement(node, 0); }

void Writer::text(const char *value) {
  out_.write(value, static_cast<std::streamsize>(
                        std::char_traits<char>::length(value)));
}

void Writer::character(char value) { out_.put(value); }

void Writer::indent(size_t level) {
  for (size_t i = 0; i < level; ++i)
    text("    ");
}

bool Writer::writeBlock(const HIRNode *node, size_t level,
                        const HIRNode *trailingStatement) {
  text("{\n");
  if (node) {
    if (node->kind == AST_BLOCK && node->blockStmts) {
      for (const HIRNode *statement : *node->blockStmts) {
        if (!writeStatement(statement, level + 1))
          return false;
      }
    } else if (!writeStatement(node, level + 1)) {
      return false;
    }
  }
  if (trailingStatement &&
      !writeStatement(trailingStatement, level + 1))
    return false;
  indent(level);
  character('}');
  return true;
}

bool Writer::writeBlock(const std::vector<HIRNode *> &statements,
                        size_t level) {
  text("{\n");
  for (const HIRNode *statement : statements) {
    if (!writeStatement(statement, level + 1))
      return false;
  }
  indent(level);
  character('}');
  return true;
}

bool Writer::writeStatement(const HIRNode *node, size_t level) {
  if (!node)
    return true;
  indent(level);
  switch (node->kind) {
  case AST_BLOCK:
    if (!node->blockStmts)
      return true;
    for (const HIRNode *statement : *node->blockStmts) {
      if (!writeStatement(statement, level))
        return false;
    }
    return true;
  case AST_ASSIGN:
    if (node->assign.isDec) {
      text("var ");
      if (!node->hasMutableInfo || node->isMutable)
        text("mut ");
      if (node->isDynamic)
        text("any ");
      else if (node->type && node->type->base != UNKNOWN) {
        if (!writeType(out_, node->type))
          return false;
        character(' ');
      }
    }
    if (!writeExpression(node->assign.target))
      return false;
    text(" = ");
    if (!writeExpression(node->assign.value))
      return false;
    text(";\n");
    return true;
  case AST_FN:
    text("fn ");
    if (node->type && node->type->base != VOID) {
      if (!writeType(out_, node->type))
        return false;
      character(' ');
    }
    if (!node->fn.name || !node->fn.params || !node->fn.body)
      return false;
    writeName(node->fn.name);
    character('(');
    {
      bool first = true;
      for (const SA::Param *param : *node->fn.params) {
        if (!param || !param->name)
          return false;
        if (std::strchr(param->name, '\003'))
          continue;
        if (!first)
          text(", ");
        first = false;
        text("var ");
        const SA::Type *paramType = param->type;
        if (param->is_variadic && paramType && paramType->base == LIST &&
            paramType->inner)
          paramType = paramType->inner;
        if (!paramType || paramType->base == ANY) {
          if (param->is_variadic)
            text("[]");
        } else {
          if (!writeType(out_, paramType))
            return false;
          if (param->is_variadic)
            text("[]");
          character(' ');
        }
        writeName(param->name);
      }
    }
    text(") ");
    if (!writeBlock(*node->fn.body, level))
      return false;
    character('\n');
    return true;
  case AST_RETURN:
    text("return");
    if (node->ret.value) {
      character(' ');
      if (!writeExpression(node->ret.value))
        return false;
    }
    text(";\n");
    return true;
  case AST_IF:
    text("if ");
    if (!writeExpression(node->ifStmt.cond))
      return false;
    character(' ');
    if (!writeBlock(node->ifStmt.thenBranch, level))
      return false;
    if (node->ifStmt.elseBranch) {
      text(" else ");
      if (!writeBlock(node->ifStmt.elseBranch, level))
        return false;
    }
    character('\n');
    return true;
  case AST_WHILE:
    text("while ");
    if (!writeExpression(node->whileLoop.cond))
      return false;
    character(' ');
    if (!writeBlock(node->whileLoop.body, level, node->whileLoop.expr))
      return false;
    character('\n');
    return true;
  case AST_CONTINUE:
    text("continue;\n");
    return true;
  case AST_BREAK:
    text("break;\n");
    return true;
  case AST_IMPORT:
    text("import ");
    if (!writeEscaped(out_, node->name, '"'))
      return false;
    text(";\n");
    return true;
  case AST_STRUCT:
    if (!node->structDec.name || !node->structDec.fields)
      return false;
    text("struct ");
    writeName(node->structDec.name);
    text(" {\n");
    for (const Fields *field : *node->structDec.fields) {
      if (!field)
        return false;
      text(field->isMut ? "var mut " : "var ");
      if (!writeType(out_, field->type))
        return false;
      character(' ');
      text(field->name.c_str());
      if (field->nodePtr.hirNode) {
        text(" = ");
        if (!writeExpression(field->nodePtr.hirNode))
          return false;
      }
      text(";\n");
    }
    indent(level);
    text("}\n");
    return true;
  case AST_CALL:
    if (!writeExpression(node))
      return false;
    text(";\n");
    return true;
  case AST_UNOP:
  case AST_BINOP:
  case AST_VAR:
  case AST_NUM:
  case AST_STR:
  case AST_CHAR:
  case AST_BOOL:
  case AST_LIST:
  case AST_INDEX:
  case AST_MEMBER:
  case AST_STRUCT_INIT:
  case AST_RANGE:
    if (!writeExpression(node))
      return false;
    text(";\n");
    return true;
  default:
    return false;
  }
}

} // namespace DesugaredSource
