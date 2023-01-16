#!/bin/bash
for filename in ../../../pyzx/demos/extracted_circuit_FRbyQsyn/*.qasm; do
#for filename in ../../benchmark/benchmark_SABRE/large/*.qasm; do
    python3 dof_generator.py $filename
    ../../qsyn -f countGates.dof >> countGates.log 2>&1
done