#include "DesugaredSourceWriter.hpp"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <string>

namespace DesugaredSource {
namespace {

const char *typeName(DataTypes type) {
  switch (type) {
  case I8: return "i8";
  case I16: return "i16";
  case I32: return "i32";
  case I64: return "i64";
  case I128: return "i128";
  case F32: return "f32";
  case F64: return "f64";
  case F128: return "f128";
  case UF32: return "uf32";
  case UF64: return "uf64";
  case UF128: return "uf128";
  case U8: return "u8";
  case U16: return "u16";
  case U32: return "u32";
  case U64: return "u64";
  case U128: return "u128";
  case PTR: return "ptr";
  case LIST: return "list";
  case RANGE: return "range";
  case BOOL: return "bool";
  case STRINGS: return "str";
  case CHARACTER: return "char";
  case VOID: return "void";
  case UNKNOWN: return "unknown";
  case ANY: return "any";
  case POISON: return nullptr;
  }
  return nullptr;
}

std::string int128ToString(__int128 value) {
  if (value == 0)
    return "0";
  const bool negative = value < 0;
  unsigned __int128 magnitude =
      negative ? static_cast<unsigned __int128>(-(value + 1)) + 1
               : static_cast<unsigned __int128>(value);
  std::string result;
  while (magnitude) {
    result.push_back(static_cast<char>('0' + magnitude % 10));
    magnitude /= 10;
  }
  if (negative)
    result.push_back('-');
  std::reverse(result.begin(), result.end());
  return result;
}

std::string uint128ToString(unsigned __int128 value) {
  if (value == 0)
    return "0";
  std::string result;
  while (value) {
    result.push_back(static_cast<char>('0' + value % 10));
    value /= 10;
  }
  std::reverse(result.begin(), result.end());
  return result;
}

} // namespace

bool writeType(std::ostream &out, const SA::Type *type) {
  if (!type)
    return false;
  if (type->kind == SA::USER_DEFINED) {
    if (!type->objType)
      return false;
    out << *type->objType;
    return true;
  }
  if (type->base == PTR) {
    if (!writeType(out, type->inner))
      return false;
    out << '&';
    return true;
  }
  if (type->base == LIST) {
    if (!writeType(out, type->inner))
      return false;
    out << '[';
    if (type->size)
      out << ';' << type->size;
    out << ']';
    return true;
  }

  const char *name = typeName(type->base);
  if (!name || type->base == UNKNOWN)
    return false;
  out << name;
  return true;
}

bool writeEscaped(std::ostream &out, const char *text, char quote) {
  if (!text)
    return false;
  out << quote;
  for (const unsigned char *p =
           reinterpret_cast<const unsigned char *>(text);
       *p; ++p) {
    switch (*p) {
    case '\\': out << "\\\\"; break;
    case '\n': out << "\\n"; break;
    case '\r': out << "\\r"; break;
    case '\t': out << "\\t"; break;
    default:
      if (*p == static_cast<unsigned char>(quote))
        out << '\\' << quote;
      else
        out << static_cast<char>(*p);
      break;
    }
  }
  out << quote;
  return true;
}

const char *operatorName(OP_kind_t op) {
  switch (op) {
  case OP_ADD: return "+";
  case OP_SUB: return "-";
  case OP_MUL: return "*";
  case OP_DIV: return "/";
  case OP_MOD: return "%";
  case OP_POW: return "**";
  case OP_AND: return "&&";
  case OP_OR: return "||";
  case OP_NOT: return "!";
  case OP_EQ: return "==";
  case OP_NEQ: return "!=";
  case OP_LT: return "<";
  case OP_LE: return "<=";
  case OP_GT: return ">";
  case OP_GE: return ">=";
  case OP_LSHIFT: return "<<";
  case OP_RSHIFT: return ">>";
  case OP_BITAND: return "&";
  case OP_BITOR: return "|";
  case OP_BITXOR: return "^";
  case OP_BITNOT: return "~";
  case OP_NEG: return "-";
  case OP_POS: return "+";
  case OP_ASSIGN: return "=";
  case OP_PLUS_ASSIGN: return "+=";
  case OP_MINUS_ASSIGN: return "-=";
  case OP_MUL_ASSIGN: return "*=";
  case OP_DIV_ASSIGN: return "/=";
  case OP_MOD_ASSIGN: return "%=";
  case OP_POW_ASSIGN: return "**=";
  case OP_LSHIFT_ASSIGN: return "<<=";
  case OP_RSHIFT_ASSIGN: return ">>=";
  case OP_INC: return "++";
  case OP_DEC: return "--";
  case OP_ADDR: return "&";
  case OP_DEREF: return "*";
  }
  return nullptr;
}

bool Writer::writeNumber(const HIRNode *node) {
  if (!node->type || node->type->kind != SA::PRIMITIVE)
    return false;
  switch (node->type->base) {
  case F32: out_ << node->val.f32; return true;
  case F64: out_ << node->val.f64; return true;
  case F128: out_ << node->val.f128; return true;
  case UF32: out_ << node->val.f32; return true;
  case UF64: out_ << node->val.f64; return true;
  case UF128: out_ << node->val.f128; return true;
  case U8: out_ << static_cast<unsigned>(node->val.u8); return true;
  case U16: out_ << node->val.u16; return true;
  case U32: out_ << node->val.u32; return true;
  case U64: out_ << node->val.u64; return true;
  case U128: out_ << uint128ToString(node->val.u128); return true;
  case I128: out_ << int128ToString(node->val.i128); return true;
  case I8: out_ << static_cast<int>(node->val.i8); return true;
  case I16: out_ << node->val.i16; return true;
  case I32: out_ << node->val.i32; return true;
  case I64: out_ << node->val.i64; return true;
  default: return false;
  }
}

} // namespace DesugaredSource
