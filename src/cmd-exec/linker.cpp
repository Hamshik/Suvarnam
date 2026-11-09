#include "linker.hpp"

#include "cmd-exec/compiler.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

extern std::vector<std::string> irOut;

namespace {

std::string makeObjectPath(const std::string &inputPath) {
  std::string objectPath = inputPath;
  if (objectPath.size() >= 3 &&
      objectPath.substr(objectPath.size() - 3) == ".ll")
    objectPath.resize(objectPath.size() - 3);
  objectPath += ".o";
  return objectPath;
}

} // namespace

int compileAndLink(const std::string &mainIrPath, const std::string &outputPath,
                   bool emitIr) {
  std::string compilerPath;
  if (!compiler::findCompatibleClang(compilerPath))
    return 1;

  const std::filesystem::path outputDirectory =
      std::filesystem::path(outputPath).parent_path();
  if (!outputDirectory.empty()) {
    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);
    if (error) {
      fprintf(stderr, "Failed to create output directory '%s': %s\n",
              outputDirectory.c_str(), error.message().c_str());
      return 1;
    }
  }

  std::vector<std::string> irPaths{mainIrPath};
  irPaths.insert(irPaths.end(), irOut.begin(), irOut.end());
  std::vector<std::string> objectPaths;
  const auto cleanup = [&] {
    for (const auto &objectPath : objectPaths)
      unlink(objectPath.c_str());
    if (!emitIr)
      unlink(mainIrPath.c_str());
  };

  for (const auto &irPath : irPaths) {
    const std::string objectPath = makeObjectPath(irPath);
    objectPaths.push_back(objectPath);
    if (!compiler::compileIrToObject(compilerPath, irPath, objectPath)) {
      cleanup();
      return 1;
    }
  }

  std::vector<std::string> linkArgs = {compilerPath, "lib/helper/StrHelper.cpp"};
  for (const auto &objectPath : objectPaths)
    linkArgs.push_back(objectPath);
  linkArgs.insert(linkArgs.end(), {"-g", "-O0", "-o", outputPath});

  const int linkResult = compiler::runCommand(std::move(linkArgs));
  cleanup();
  return linkResult == 0 ? 0 : 1;
}
