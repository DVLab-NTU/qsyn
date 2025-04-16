#! /usr/bin/env bash

# in /app
cmake -B /app/build -S /app/qsyn || exit 1
cmake --build /app/build --parallel "$(nproc)" || exit 1

cd /app/qsyn || exit 1
/app/build/qsyn-unit-test || exit 1
/app/qsyn/scripts/RUN_TESTS --qsyn /app/build/qsyn "$@"
