#!/bin/bash
set -e
set -o pipefail

# Determine absolute path to the project root (parent of tools/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

echo "==> Project root: $PROJECT_ROOT"
echo "==> Build dir:    $BUILD_DIR"

# echo "==> Removing old build directory (if exists)..."
# rm -rf "$BUILD_DIR"

# echo "==> Creating new build directory..."
# mkdir -p "$BUILD_DIR"

echo "==> Running CMake configuration with tests enabled..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DETOOLS_BUILD_TESTS=ON

echo "==> Building project..."
cmake --build "$BUILD_DIR" -- -j$(nproc)

echo "==> Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure
