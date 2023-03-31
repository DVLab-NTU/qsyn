#!/bin/bash
doit() {
    ./qsyn -f $1 > $2 2>&1
}
export -f doit
doit-mimalloc() {
    LD_PRELOAD=libmimalloc.so ./qsyn -f $1 > $2 2>&1
}
export -f doit-mimalloc

if [ $# -ne 1 ]; then
    echo "Error: wrong number of arguments!"
    echo "Usage: ./overhaul_benchmark.sh <path/to/dir>"
    exit 1
fi

FOLDER=$1
mkdir -p ref/${FOLDER}

# ls ref/freduce/*.dof | parallel doit {} ref/${FOLDER}/{/.}.log
ls ref/freduce/*.dof | parallel doit-mimalloc {} ref/${FOLDER}/{/.}.log
