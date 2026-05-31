#! /usr/bin/env bash

# in /app
cmake -B /app/build -S /app/qsyn || exit 1
if ! cmake --build /app/build --parallel "$(nproc)"; then
    echo "=== Build failed; last verbose output (single-threaded) ==="
    cmake --build /app/build --parallel 1 --verbose 2>&1 | tail -80
    exit 1
fi

cd /app/qsyn || exit 1
/app/build/qsyn-unit-test || exit 1
/app/qsyn/scripts/RUN_TESTS --qsyn /app/build/qsyn "$@"
