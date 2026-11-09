#pragma once

#include <string>

int compileAndLink(const std::string &mainIrPath,
                   const std::string &outputPath, bool emitIr);
