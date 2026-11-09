#include "cmd-exec/cmd-exec.hpp"
#include "semantic/import.hpp"
#include "utils/error_handler/error.h"

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <limits.h>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>

std::vector<char *> importSources{};
bool nonStaticType = false;
bool emitDesugaredSource = false;

namespace {

constexpr const char *usage =
    "Usage: SA [source] [-o bin_path] [--emit-ir ir_path] "
    "[--emit-desugared source_path] [-I directory] [--non-static-mod]";

bool readOptionValue(int argc, char **argv, int &index, const char *option,
                     std::string *value) {
  if (index + 1 >= argc) {
    syserr((std::string("Missing argument for ") + option + "\n" + usage)
               .c_str());
    return false;
  }

  *value = argv[index + 1];
  index += 2;
  return true;
}

bool addImportDirectory(char *path) {
  if (!std::filesystem::is_directory(path)) {
    syserr((std::string("Missing directory: ") + path).c_str());
    return false;
  }
  importSources.push_back(path);
  return true;
}

FILE *openFile(const char *filename, char **resolvedPathOut) {
  char resolved[PATH_MAX];
  const char *openPath = filename;
  if (realpath(filename, resolved))
    openPath = resolved;

  FILE *input = fopen(openPath, "rb");
  if (!input) {
    int savedErrno = err_no;
    fprintf(stderr, "Failed to open input: %s\n", filename);
    if (openPath != filename)
      fprintf(stderr, "Resolved path: %s\n", openPath);
    err_no = savedErrno;
    perror("fopen");
    return nullptr;
  }
  if (resolvedPathOut)
    *resolvedPathOut = strdup(openPath);
  return input;
}

} // namespace

bool parseArguments(int argc, char **argv, Options *options) {
  *options = Options{};
  nonStaticType = false;
  emitDesugaredSource = false;

  if (argc > 1 && argv[1][0] != '-')
    options->inputFilename = argv[1];

  using OptionHandler = std::function<bool(int &, int, char **)>;
  const std::unordered_map<std::string, OptionHandler> optionHandlers = {
      {"-o",
       [options](int &index, int argCount, char **args) {
         if (!readOptionValue(argCount, args, index, "-o",
                              &options->binOutputPath))
           return false;
         if (!Importer::isexists(options->binOutputPath))
           syserr(("Directory does not exist: " +
                   options->binOutputPath)
                      .c_str());
         return true;
       }},
      {"--emit-desugared",
       [options](int &index, int argCount, char **args) {
         if (!readOptionValue(argCount, args, index, "--emit-desugared",
                              &options->desugaredSourcePath))
           return false;
         if (!Importer::isexists(options->desugaredSourcePath))
           syserr(("Directory does not exist: " +
                   options->desugaredSourcePath)
                      .c_str());
         options->emitDesugaredSource = true;
         emitDesugaredSource = true;
         return true;
       }},
      {"--emit-ir",
       [options](int &index, int argCount, char **args) {
         if (!readOptionValue(argCount, args, index, "--emit-ir",
                              &options->irOutputPath))
           return false;
         options->emitIr = true;
         if (!Importer::isexists(options->irOutputPath))
           syserr(("Directory does not exist: " +
                   options->irOutputPath)
                      .c_str());
         return true;
       }},
      {"-I",
       [](int &index, int argCount, char **args) {
         char *path = args[index] + 2;
         if (*path != '\0') {
           ++index;
         } else {
           if (index + 1 >= argCount) {
             syserr("Missing directory after -I");
             return false;
           }
           path = args[index + 1];
           index += 2;
         }
         return addImportDirectory(path);
       }},
      {"--non-static-mod",
       [](int &index, int, char **) {
         nonStaticType = true;
         ++index;
         return true;
       }},
  };

  for (int index = options->inputFilename.empty() ? 1 : 2;
       index < argc;) {
    std::string option = argv[index];
    if (option.compare(0, 2, "-I") == 0)
      option = "-I";

    const auto handler = optionHandlers.find(option);
    if (handler == optionHandlers.end()) {
      syserr(logf_msg("Unknown argument: %s\n%s", argv[index], usage));
      return false;
    }
    if (!handler->second(index, argc, argv))
      return false;
  }

  if (options->emitIr) {
    char resolved[PATH_MAX];
    if (realpath(options->irOutputPath.c_str(), resolved))
      options->irOutputPath = resolved;
  }
  return true;
}

bool setupInputFile(const Options *options, File *file) {
  if (options->inputFilename.empty()) {
    file->filename = (char *)"<stdin>";
    file->source = stdin;
    return true;
  }

  char *resolvedPath = nullptr;
  FILE *input = openFile(options->inputFilename.c_str(), &resolvedPath);
  if (!input)
    return false;

  file->filename = resolvedPath ? resolvedPath
                                : (char *)options->inputFilename.c_str();
  file->source = input;
  return true;
}
