#include "semantic/import.hpp"

std::optional<fs::path>
ImportResolver::resolve(const std::string &import_path,
                        const fs::path &current_file_path) {
  fs::path target(import_path);

  // 1. Absolute Path: If the import path is already absolute, canonicalize it
  if (target.is_absolute()) {
    std::error_code ec;
    auto canonical = fs::canonical(target, ec);
    if (!ec)
      return canonical;
  }

  fs::path current_dir = fs::absolute(current_file_path).parent_path();
  fs::path relative_candidate = current_dir / target;

  std::error_code ec;
  auto canonical_relative = fs::canonical(relative_candidate, ec);
  if (!ec) {
    return canonical_relative;
  }

  for (const auto &include_dir : *include_paths) {
    fs::path candidate = include_dir / target;
    auto canonical_candidate = fs::canonical(candidate, ec);
    if (!ec) {
      return canonical_candidate;
    }
  }

  return std::nullopt;
}