#! /usr/bin/env bash

cmake -B ./build -S ./qsyn -DCOPY_EXECUTABLE=OFF
cmake --build ./build --parallel "$(nproc)"
./qsyn/scripts/RUN_TESTS --qsyn ./build/qsyn "$@"
