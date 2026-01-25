#!/bin/bash
# Build script for VKCompute project

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="${1:-Release}"

echo "========================================"
echo "  VKCompute Build Script"
echo "========================================"
echo ""
echo "Build Type: ${BUILD_TYPE}"
echo "Build Dir:  ${BUILD_DIR}"
echo ""

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
echo "Configuring..."
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ..

# Build
echo ""
echo "Building..."
cmake --build . --parallel $(nproc)

echo ""
echo "========================================"
echo "  Build completed successfully!"
echo "========================================"
echo ""
echo "Executables are in: ${BUILD_DIR}/bin/"
echo ""
ls -la "${BUILD_DIR}/bin/" 2>/dev/null || echo "(no executables yet)"
