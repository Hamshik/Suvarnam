#!/bin/bash
set -e

PARENT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PARENT_DIR/build"
mkdir -p "$BUILD_DIR"

declare -A targets=(
  ["$BUILD_DIR/Parser.ih"]="$PARENT_DIR/src/frontend/parser/Parser.ih"
  ["$BUILD_DIR/Scanner.ih"]="$PARENT_DIR/src/frontend/lexer/Scanner.ih"
  ["$BUILD_DIR/Parser.h"]="$PARENT_DIR/include/frontend/parser/Parser.h"
  ["$BUILD_DIR/Scanner.h"]="$PARENT_DIR/include/frontend/lexer/Scanner.h"
  ["$BUILD_DIR/parser.yy"]="$PARENT_DIR/src/frontend/parser/parser.yy"
)

# Expand *.ly glob explicitly (can't be done inside the array literal)
for ly in "$PARENT_DIR"/src/frontend/parser/*.ly; do
  [ -e "$ly" ] || continue
  targets["$BUILD_DIR/$(basename "$ly")"]="$ly"
done

for link in "${!targets[@]}"; do
  source="${targets[$link]}"

  # Ensure source parent dir exists
  mkdir -p "$(dirname "$source")"

  # If source is a symlink, replace it with the real file
  if [ -L "$source" ]; then
    resolved_source="$(readlink -f "$source")"
    if [ -f "$resolved_source" ]; then
      temporary_source="$(mktemp)"
      cp -p "$resolved_source" "$temporary_source"
      rm -f "$source"
      mv "$temporary_source" "$source"
    else
      rm -f "$source"
    fi
  fi

  # Create relative symlink
  relative_target="$(realpath --relative-to="$(dirname "$link")" "$source")"
  if [ -L "$link" ] && [ "$(readlink "$link")" = "$relative_target" ]; then
    echo "✓ $link → $relative_target (unchanged)"
  else
    rm -f "$link"
    ln -s "$relative_target" "$link"
    echo "✓ $link → $relative_target"
  fi
done

echo "Relative symlinks successfully created in build tree → source tree"   