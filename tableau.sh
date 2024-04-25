#!/bin/bash

for benchmark in ./benchmark/SABRE/small/*.qasm; do
    echo $benchmark
    filename=$(basename $benchmark)
    noext="${filename%.*}"
    # if [[ "$noext" == "gf2^128_mult" ]]
    # then
    #   echo "skip"
    #   continue
    # elif [[ "$noext" == "gf2^256_mult" ]]
    # then
    #   echo "skip"
    #   continue
    # elif [[ "$noext" == "gf2^64_mult" ]]
    # then
    #   echo "skip"
    #   continue
    # elif [[ "$noext" == "gf2^32_mult" ]]
    # then
    #   echo "skip"
    #   continue
    # fi

    ./qsyn -f ref/tableau.dof ${benchmark} ref/SABRE/small/${filename}.qasm > ./Benchmarks/ref/${noext}.log 2>&1
done