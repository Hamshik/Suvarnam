#include "cmd-exec/compiler.hpp"

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

bool runExecCapture(const std::string &program,
                    std::vector<std::string> arguments, std::string &output,
                    int &exitCode) {
  int pipeFds[2];
  if (pipe(pipeFds) < 0) {
    perror("pipe");
    return false;
  }

  std::vector<char *> argv;
  argv.reserve(arguments.size() + 1);
  for (std::string &argument : arguments)
    argv.push_back(argument.data());
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    close(pipeFds[0]);
    if (dup2(pipeFds[1], STDOUT_FILENO) < 0 ||
        dup2(pipeFds[1], STDERR_FILENO) < 0)
      _exit(126);
    close(pipeFds[1]);
    execvp(program.c_str(), argv.data());
    _exit(127);
  }
  close(pipeFds[1]);
  if (pid < 0) {
    perror("fork");
    close(pipeFds[0]);
    return false;
  }

  char buffer[256];
  bool readOk = true;
  while (true) {
    ssize_t bytesRead = read(pipeFds[0], buffer, sizeof(buffer));
    if (bytesRead > 0) {
      output.append(buffer, static_cast<size_t>(bytesRead));
      continue;
    }
    if (bytesRead == 0)
      break;
    if (errno == EINTR)
      continue;
    perror("read");
    readOk = false;
    break;
  }
  close(pipeFds[0]);

  int status;
  pid_t waited;
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited < 0) {
    perror("waitpid");
    return false;
  }
  exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  return readOk;
}

bool clangMajorVersion(const std::string &compiler, int &major,
                       std::string &reason) {
  std::string output;
  int exitCode = 0;
  if (!runExecCapture(compiler, {compiler, "--version"}, output, exitCode)) {
    reason = "could not query version";
    return false;
  }
  if (exitCode == 127) {
    reason = "not found on PATH";
    return false;
  }
  if (exitCode != 0) {
    reason = "version query failed";
    return false;
  }

  std::string lowercase = output;
  for (char &character : lowercase)
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  if (lowercase.find("clang") == std::string::npos) {
    reason = "does not identify itself as Clang";
    return false;
  }

  const size_t versionPos = lowercase.find("version ");
  size_t digitPos = versionPos == std::string::npos
                        ? std::string::npos
                        : versionPos + sizeof("version ") - 1;
  if (digitPos == std::string::npos ||
      !std::isdigit(static_cast<unsigned char>(lowercase[digitPos]))) {
    reason = "could not parse version output";
    return false;
  }

  major = 0;
  while (digitPos < lowercase.size() &&
         std::isdigit(static_cast<unsigned char>(lowercase[digitPos]))) {
    major = major * 10 + lowercase[digitPos] - '0';
    ++digitPos;
  }
  if (major < 22) {
    reason =
        "found Clang " + std::to_string(major) + ", need version 22 or newer";
    return false;
  }
  return true;
}

} // namespace

namespace compiler {

bool findCompatibleClang(std::string &compilerPath) {
  std::string reason;
  int major = 0;
  if (clangMajorVersion("clang++-22", major, reason)) {
    compilerPath = "clang++-22";
    return true;
  }
  const std::string clang22Reason = reason;

  major = 0;
  if (clangMajorVersion("clang++", major, reason)) {
    compilerPath = "clang++";
    return true;
  }

  fprintf(stderr,
          "Compatible Clang compiler not found. Install clang++-22 or "
          "provide clang++ version 22 or newer.\n"
          "clang++-22: %s\nclang++: %s\n",
          clang22Reason.c_str(), reason.c_str());
  return false;
}

bool compileIrToObject(const std::string &compilerPath,
                       const std::string &irPath,
                       const std::string &objectPath) {
  std::vector<std::string> args = {compilerPath, "-c", "-x", "ir",
                                   irPath,       "-o", objectPath};
  return runCommand(std::move(args)) == 0;
}

int runCommand(std::vector<std::string> arguments) {
  std::vector<char *> argv;
  argv.reserve(arguments.size() + 1);
  for (std::string &argument : arguments)
    argv.push_back(argument.data());
  if (argv.empty())
    return -1;
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    execvp(argv[0], argv.data());
    _exit(127);
  }
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  int status;
  pid_t waited;
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited < 0) {
    perror("waitpid");
    return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

} // namespace compiler
