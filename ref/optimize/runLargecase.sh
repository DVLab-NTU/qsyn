#!/bin/bash
for filename in ../../benchmark/benchmark_SABRE/large/*.qasm; do
    python3 writedof.py $filename
    ../../qsyn -f largecase.dof >> largecaselogs/largecase_stats.log 2>&1
done