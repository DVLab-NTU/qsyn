#!/bin/bash
doit() {
    ./qsyn -f $1 > $2 2>&1
}
export -f doit
doit-mimalloc() {
    LD_PRELOAD=libmimalloc.so ./qsyn -f $1 > ref/$2.log 2>&1
}
export -f doit-mimalloc

if [ $# -ne 1 ]; then
    echo "Error: wrong number of arguments!"
    echo "Usage: ./overhaul_benchmark.sh <path/to/dir>"
    exit 1
fi

FOLDER=$1
mkdir -p ref/${FOLDER}
mkdir -p ref/${FOLDER}-mimalloc
# parallel LD_PRELOAD=libmimalloc.so ./qsyn -f {} ::: ${files} > ref/${FOLDER}-mimalloc/{}.log 2>&1
ls ref/freduce/*.dof | parallel doit {} ref/${FOLDER}/{/.}.log
ls ref/freduce/*.dof | parallel doit-mimalloc {} ref/${FOLDER}-mimalloc/{/.}.log
# ls ref/freduce/*.dof | parallel echo {} $(basename {}) # ./qsyn -f {} > ref/${FOLDER}-mimalloc/{}.log 2>&1
# for f in ref/freduce/*; do
#     echo ${f}
#     # ./qsyn -f "ref/${file}.dof" > ref/${FOLDER}/${file}.log 2>&1
#     # LD_PRELOAD=libmimalloc.so ./qsyn -f "ref/${file}.dof" > ref/${FOLDER}-mimalloc/${file}.log 2>&1
# done