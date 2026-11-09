#include "linker.hpp"

#include "cmd-exec/cmd-exec.hpp"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
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

bool getClangMajorVersion(const std::string &compiler, int &majorVersion) {
  int outputPipe[2];
  if (pipe(outputPipe) < 0) {
    perror("pipe");
    return false;
  }

  pid_t pid = fork();
  if (pid == 0) {
    close(outputPipe[0]);
    if (dup2(outputPipe[1], STDOUT_FILENO) < 0)
      _exit(126);
    close(outputPipe[1]);

    char *const arguments[] = {const_cast<char *>(compiler.c_str()),
                               const_cast<char *>("--version"), nullptr};
    execvp(arguments[0], arguments);
    _exit(127);
  }
  close(outputPipe[1]);
  if (pid < 0) {
    perror("fork");
    close(outputPipe[0]);
    return false;
  }

  std::string output;
  char buffer[256];
  ssize_t bytesRead;
  while ((bytesRead = read(outputPipe[0], buffer, sizeof(buffer))) > 0) {
    if (output.size() < 4096)
      output.append(buffer, static_cast<size_t>(bytesRead));
  }
  if (bytesRead < 0)
    perror("read");
  close(outputPipe[0]);

  int status;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR)
      continue;
    perror("waitpid");
    return false;
  }
  if (bytesRead < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return false;

  const std::string versionMarker = "clang version ";
  const size_t markerPosition = output.find(versionMarker);
  if (markerPosition == std::string::npos)
    return false;

  const char *versionStart =
      output.c_str() + markerPosition + versionMarker.size();
  char *versionEnd = nullptr;
  long version = std::strtol(versionStart, &versionEnd, 10);
  if (versionEnd == versionStart || version < 0)
    return false;

  majorVersion = static_cast<int>(version);
  return true;
}

std::string findClangCompiler() {
  int majorVersion;
  if (getClangMajorVersion("clang++-22", majorVersion) && majorVersion == 22)
    return "clang++-22";
  if (getClangMajorVersion("clang++", majorVersion) && majorVersion > 22)
    return "clang++";

  fprintf(stderr,
          "Error: requires clang++-22 or clang++ with a version greater than "
          "22, but neither suitable compiler was found on PATH.\n");
  return {};
}

int runExec(const char *program, char *const arguments[]) {
  pid_t pid = fork();
  if (pid == 0) {
    execvp(program, arguments);
    _exit(127);
  }
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  int status;
  if (waitpid(pid, &status, 0) < 0) {
    perror("waitpid");
    return -1;
  }
  
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return 1;
}

bool compileIrToObject(const std::string &compiler, const std::string &irPath,
                       const std::string &objectPath) {
  char const *args[] = {
      compiler.c_str(),  "-c", "-x", "ir", irPath.c_str(), "-o",
      objectPath.c_str(), nullptr};

  return runExec(args[0], (char *const *)args) == 0;
}

} // namespace

int compileAndLink(const std::string &mainIrPath, const std::string &outputPath,
                   bool emitIr) {
  const std::string compiler = findClangCompiler();
  if (compiler.empty())
    return 1;

  size_t lastSlash = outputPath.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    std::string directoryPath = outputPath.substr(0, lastSlash);
    std::string mkdirCommand = "mkdir -p " + directoryPath;
    system(mkdirCommand.c_str());
  }

  std::vector<std::string> objectPaths;
  const std::string mainObjectPath = makeObjectPath(mainIrPath);
  if (!compileIrToObject(compiler, mainIrPath, mainObjectPath)) {
    if (!emitIr)
      unlink(mainIrPath.c_str());
    return 1;
  }
  objectPaths.push_back(mainObjectPath);

  for (const auto &importIr : irOut) {
    const std::string importObjectPath = makeObjectPath(importIr);
    if (!compileIrToObject(compiler, importIr, importObjectPath)) {
      if (!emitIr)
        unlink(mainIrPath.c_str());
      return 1;
    }
    objectPaths.push_back(importObjectPath);
  }

  std::vector<std::string> linkArgs = {compiler, "lib/helper/StrHelper.cpp"};
  for (const auto &objectPath : objectPaths)
    linkArgs.push_back(objectPath);
  linkArgs.insert(linkArgs.end(), {"-g", "-O0", "-o", outputPath});

  std::vector<char *> linkArguments;
  linkArguments.reserve(linkArgs.size() + 1);
  for (auto &arg : linkArgs)
    linkArguments.push_back(arg.data());
  linkArguments.push_back(nullptr);

  int linkResult = runExec(linkArguments[0], linkArguments.data());

  if (!emitIr)
    unlink(mainIrPath.c_str());

  for (const auto &objectPath : objectPaths)
    unlink(objectPath.c_str());

  return linkResult == 0 ? 0 : 1;
}
