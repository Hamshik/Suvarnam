#pragma once

#include "enums.h"
#include <cstddef>
#include <sstream>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct File {
  char *filename;
  FILE *source;
};

extern File *file;
extern size_t err_no;
extern size_t warn_no;
extern bool isError;
extern bool isWarning;
extern bool error_fatal;

class Semantic;
class SemanticSymTable;
class Scanner;
class Parser;
class Importer;
struct ASTNode;

namespace SA {
/* Extended source location that includes absolute byte offsets. */
struct Location {
  size_t firstLn;
  size_t firstCol;
  size_t firstPos; /* 0-based byte offset */
  size_t lastLn;
  size_t lastCol;
  size_t lastPos; /* 0-based byte offset */

};

inline Location operator+(const Location& a, const Location& b) {
    return Location{
      .firstLn = a.firstLn,
      .firstCol = a.firstCol,
      .firstPos = a.firstPos,
      .lastLn = b.lastLn,
      .lastCol = b.lastCol,
      .lastPos = b.lastPos
    };
}

struct SA_Ptr {
  size_t frame_id;
  char *name;
};

struct SA_Range {
  int64_t start;
  int64_t end;
  int64_t step;
};

union Value {
  int8_t i8;
  short i16;
  int i32;
  long int i64;
  __int128 i128;
  float f32;
  double f64;
  long double f128;
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  unsigned __int128 u128;
  SA_Ptr ptr;
  SA_Range range;
  bool bval;
  char *chars;
  void *raw;
};

struct idxExpr {
  ::ASTNode *exprNode; // for expr like [i[0] + 1] ect
  int depth;                 // to know how much use goes like this i[][][]...
  bool isglobal;
  struct idxExpr *next; // next of i[]of i[][]... <- this one
};

struct Type {
  DataTypes_t base;    // e.g., LIST, PTR, INT
  Type *inner; // Points to the next type (recursive)
  size_t size = 0;
  bool ismut = false;

  Type(DataTypes_t base, Type *inner) : base(base), inner(inner) {}
};

struct TypedVal {
  Type *type;
  SA::Value val;
};

struct Param {
  char *name;
  Type *type;
  bool is_variadic;
  // Default constructor: safely zero out everything
  Param() : name(nullptr), type(nullptr), is_variadic(false) {}

  // SA::Type constructor: ensure non-pointer fields aren't filled with junk
  // data
  Param(SA::Type *type) : name(nullptr), type(type), is_variadic(false) {}

  // Variadic helper constructor (useful for built-ins like printf)
  Param(bool variadic) : name(nullptr), type(nullptr), is_variadic(variadic) {}

  Param(bool variadic, SA::Type *types)
      : name(nullptr), type(types), is_variadic(variadic) {}
};

struct ParamList {
  Param *params;
  int count;
};

class CompilerContext {
public:
  Semantic *semantic = nullptr;
  SemanticSymTable *sym = nullptr;
  // Pass stream directly to Scanner instead of messing with std::cin
  std::istringstream& input;
  Scanner *scanner;
  Parser *parser;

  explicit CompilerContext(
    std::istringstream &input,
    SemanticSymTable *sym = nullptr, Scanner *scanner = nullptr,
    Parser *parser = nullptr, Semantic *semantic = nullptr
  );
  ~CompilerContext();

  void setup(Importer* importer);
};

}
#include "nodes.h"