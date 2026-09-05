#!/bin/bash
set -e

BUILD_DIR="build"
mkdir -p "$BUILD_DIR"

# Force relative or absolute symlinks from src to build
ln -sf "$(pwd)/include/frontend/parser/Parser.h"  "$BUILD_DIR/Parser.h"
ln -sf "$(pwd)/include/frontend/parser/Parser.ih" "$BUILD_DIR/Parser.ih"

ln -sf "$(pwd)/include/frontend/lexer/Scanner.h"  "$BUILD_DIR/Scanner.h"
ln -sf "$(pwd)/include/frontend/lexer/Scanner.ih" "$BUILD_DIR/Scanner.ih"

echo "Symlinks successfully created in $BUILD_DIR/"