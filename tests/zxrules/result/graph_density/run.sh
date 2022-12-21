#!/bin/bash
for filename in /home/arttr1521/git/qsyn/benchmark/benchmark_SABRE/small/*; do
    #echo "******** f= $filename ************"
    python3 ./tests/zxrules/result/graph_density/gen_result.py $filename
    ./qsyn -f ./tests/zxrules/result/dof/FR.dof > ./tests/zxrules/result/ref/FR.log
    python3 ./tests/zxrules/result/graph_density/calculus_density.py ./tests/zxrules/result/ref/FR.log $filename >> ./tests/zxrules/result/log.log
done
