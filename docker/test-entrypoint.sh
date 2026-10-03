#! /usr/bin/env bash
set -euo pipefail

# Ensure clean build directory to avoid CMake cache issues
# CMake caches compiler choice in CMakeCache.txt, so we need to remove
# the build directory to ensure we use the compiler specified in ENV
rm -rf /app/build

# Get compiler paths from environment (set in Dockerfile)
# Use absolute paths to ensure we get the right compiler
CC_COMPILER="${CC:-/usr/bin/gcc}"
CXX_COMPILER="${CXX:-/usr/bin/g++}"

# If CC/CXX are not set, try to infer from which command
if [ -z "${CC:-}" ] || [ -z "${CXX:-}" ]; then
    # Check if we're in a gcc or clang container by checking the Dockerfile ENV
    # This is a fallback if ENV variables aren't properly set
    if command -v g++ >/dev/null 2>&1 && command -v clang++ >/dev/null 2>&1; then
        # Both exist, prefer g++ for gcc-test, clang++ for clang-test
        # Check the actual executable to determine
        if [ -f "/usr/bin/g++" ]; then
            CC_COMPILER="/usr/bin/gcc"
            CXX_COMPILER="/usr/bin/g++"
        elif [ -f "/usr/bin/clang++" ]; then
            CC_COMPILER="/usr/bin/clang"
            CXX_COMPILER="/usr/bin/clang++"
        fi
    fi
fi

# Verify compiler exists and is executable
if [ ! -x "$CC_COMPILER" ]; then
    echo "Error: C compiler not found or not executable: $CC_COMPILER"
    exit 1
fi
if [ ! -x "$CXX_COMPILER" ]; then
    echo "Error: C++ compiler not found or not executable: $CXX_COMPILER"
    exit 1
fi

# Print compiler info for debugging
echo "=== Compiler Configuration ==="
echo "CC: $CC_COMPILER ($($CC_COMPILER --version | head -1))"
echo "CXX: $CXX_COMPILER ($($CXX_COMPILER --version | head -1))"
echo "==============================="

# Explicitly specify compiler to ensure deterministic builds
# This overrides any CMake cache and ensures we use the compiler
# specified in the Dockerfile (gcc/g++ or clang/clang++)
cmake -B /app/build -S /app/qsyn \
  -DCMAKE_C_COMPILER="$CC_COMPILER" \
  -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
  -DCMAKE_BUILD_TYPE=Release

# Verify compiler selection (for debugging)
echo ""
echo "=== CMake Compiler Selection ==="
grep -E "CMAKE_(C|CXX)_COMPILER:" /app/build/CMakeCache.txt || true
echo "================================"
echo ""

cmake --build /app/build --parallel "$(nproc)"

cd /app/qsyn || exit 1
/app/build/qsyn-unit-test || exit 1
/app/qsyn/scripts/RUN_TESTS --qsyn /app/build/qsyn "$@"
