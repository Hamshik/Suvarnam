#pragma once

#include <string>
#include <vector>

namespace compiler {

bool findCompatibleClang(std::string &compiler);
bool compileIrToObject(const std::string &compiler, const std::string &irPath,
                       const std::string &objectPath);
int runCommand(std::vector<std::string> arguments);

}
