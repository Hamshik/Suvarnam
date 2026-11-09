#include "HIRGen/HIRGen.hpp"
#include "DesugaredSourceWriter.hpp"

#include <fstream>
#include <iostream>

bool writeDesugaredSource(const HIRNode *root, const std::string &path) {
  std::ofstream output(path);
  if (!output) {
    std::cerr << "Error: cannot open desugared source output '" << path
              << "'\n";
    return false;
  }

  DesugaredSource::Writer writer(output);
  if (!writer.write(root)) {
    std::cerr << "Error: unsupported or incomplete HIR node while emitting "
                 "desugared source\n";
    return false;
  }
  output.flush();
  if (!output) {
    std::cerr << "Error: failed to write desugared source '" << path << "'\n";
    return false;
  }
  return true;
}
