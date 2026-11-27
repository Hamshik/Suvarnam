#include "cmd-exec/cmd-exec.hpp"
#include "parser/parser_helpers.h"
#include "semantic/import.hpp"
#include "shared/structs.h"
#include <filesystem>
#include <stdlib.h>
#include <vector>

extern std::vector<char *> importSources;

ASTNode* root{};
File *file{};

int main(int argc, char **argv) {

    file = new File;
    
    Options opts;
    if (!parseArguments(argc, argv, &opts)) {
        return EXIT_FAILURE;
    }

    if (!setupInputFile(&opts, file)) {
        return EXIT_FAILURE;
    }
    error_fatal = false;

    int status = 0;
    auto importer = new Importer(file->source);

    // Build include-path vector from -I arguments and hand it to the importer
    static std::vector<std::filesystem::path> includeDirs;
    includeDirs.clear();
    for (auto *p : importSources)
        includeDirs.emplace_back(p);
    importer->updatePaths(&includeDirs);

    importer->getCtx()->setup(importer);
    root = importer->parseFile();

    if (root && !isError) {
        status = compileAndExecute(root, &opts, importer);
    }
    
    if (file->source != stdin)
        fclose(file->source);
    if (!opts.inputFilename.empty() && file->filename &&
        file->filename != opts.inputFilename.c_str())
        free(file->filename);

    return status;
}
