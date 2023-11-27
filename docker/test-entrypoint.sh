#! /usr/bin/env bash

# in /app
cmake -B /app/build -S /app/qsyn -DCOPY_EXECUTABLE=OFF
cmake --build /app/build --parallel "$(nproc)"

cd /app/qsyn || return # cd to /app/qsyn
/app/qsyn/scripts/RUN_TESTS --qsyn /app/build/qsyn "$@"
