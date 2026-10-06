#pragma once

#include <string>

namespace compiler {

bool findCompatibleClang(std::string &compiler);
bool compileIrToObject(const std::string &compiler, const std::string &irPath,
                       const std::string &objectPath);

}
