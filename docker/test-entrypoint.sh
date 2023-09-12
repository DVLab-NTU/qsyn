#! /usr/bin/env bash
mkdir build
cd build || {
    echo "Failed to enter build directory"
    exit 1
}

cmake ../qsyn
make -j"$(nproc)"
echo ../qsyn/RUN_TESTS "$@" --qsyn ./qsyn
../qsyn/RUN_TESTS "$@" --qsyn ./qsyn
