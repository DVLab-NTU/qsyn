#! /usr/bin/env bash

cmake -B ./build -S ./qsyn
cmake --build ./build --parallel "$(nproc)"
./qsyn/scripts/RUN_TESTS --qsyn ./build/qsyn "$@"
