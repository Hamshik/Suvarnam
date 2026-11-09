#pragma once

#include "shared/HIRNode.hpp"

#include <iosfwd>

namespace DesugaredSource {

bool writeType(std::ostream &out, const SA::Type *type);
bool writeEscaped(std::ostream &out, const char *text, char quote);
const char *operatorName(OP_kind_t op);

class Writer {
public:
  explicit Writer(std::ostream &out) : out_(out) {}
  bool write(const HIRNode *node);

private:
  std::ostream &out_;

  void text(const char *value);
  void character(char value);
  void indent(size_t level);
  void writeName(const char *name);
  bool writeBlock(const HIRNode *node, size_t level);
  bool writeBlock(const std::vector<HIRNode *> &statements, size_t level);
  bool writeNumber(const HIRNode *node);
  bool writeExpression(const HIRNode *node);
  bool writeStatement(const HIRNode *node, size_t level);
};

} // namespace DesugaredSource
