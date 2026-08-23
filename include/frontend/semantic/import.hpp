#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

class ImportResolver {
private:
    std::vector<fs::path>* include_paths; // e.g., stdlib paths, -I flags

public:
    ImportResolver(std::vector<fs::path>* search_paths = {}) 
        : include_paths(std::move(search_paths)) {}

    // Main resolution logic
    std::optional<fs::path> resolve(const std::string& import_path, 
                                    const fs::path& current_file_path);
};