#! /usr/bin/env bash
set -e
set -o pipefail

BENCHMARK_DIR=./pyzx_benchmark
P_COUNT=2
DOFILE=./preduce.dof

if [ "$#" -gt 0 ]; then
    BENCHMARK_DIR=$(readlink -f "$1")
fi

if [ "$#" -eq 2 ]; then
    P_COUNT="$2"
elif [ "$#" -gt 2 ]; then
    echo "Usage: ./run.sh [benchmark_dir] [partition_count]"
    exit 1
fi

cd "$(dirname "$0")" || exit

if [ ! -d "../build" ]; then
    mkdir ../build
    cd ../build || exit
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
    cd "$(dirname "$0")" || exit
fi

make -C ../build -j6

mkdir -p ./results
export DOFILE
export P_COUNT
run_benchmark ()
{
    QASM_FILE=$1
    FILE_NAME=$(basename -- "$QASM_FILE")
    FILE_NAME=${FILE_NAME%.qasm}
    PRZX_FILE=./results/${FILE_NAME}_pr.zx
    DRZX_FILE=./results/${FILE_NAME}_dr.zx
    LOG_FILE=./results/${FILE_NAME}.log

    echo "Running $QASM_FILE"

    if ! ../qsyn -f "$DOFILE" "$QASM_FILE" "$PRZX_FILE" "$DRZX_FILE" "$P_COUNT" > "$LOG_FILE" ; then
        echo "Failed to run $QASM_FILE"
        exit 1
    fi

    # python3 ./pyzx_benchmark/run.py "$QASM_FILE"
}

export -f run_benchmark

if command -v parallel &> /dev/null; then
    echo "Using parallel"
    find "$BENCHMARK_DIR" -type f -name "*.qasm" -size -500c | parallel -j6 run_benchmark
else
    echo "parallel not found, using xargs"
    find "$BENCHMARK_DIR" -type f -name "*.qasm" -size -500c | xargs -n1 -P6 -I{} bash -c 'run_benchmark "$@"' _ {}
fi
