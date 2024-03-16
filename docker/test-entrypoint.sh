#! /usr/bin/env bash

# in /app
cmake -B /app/build -S /app/qsyn -DCOPY_EXECUTABLE=OFF || exit 1
cmake --build /app/build --parallel "$(nproc)" || exit 1

cd /app/qsyn || exit 1
/app/qsyn/scripts/RUN_TESTS --qsyn /app/build/qsyn "$@"
