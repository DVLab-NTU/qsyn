#! /usr/bin/env bash

# this file is an example of how to use qsyn in a shell script
# the file will optimize the input file with zx-based optimization and draw the output circuit using qiskit

# should have 3 arguments: input file, output file
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input file> <output file>"
    exit 1
fi

# check if qiskit is installed
if ! python3 -c "import qiskit" &> /dev/null; then
    echo "Error: qiskit is not installed"
    exit 1
fi

# cd to the root directory of this repository
cd "$(git rev-parse --show-toplevel)" || exit 1

# run qsyn with the given arguments
# the -q/--quiet flag suppresses the prompt
# the -c/--command flag runs the given command
# the qcir write command writes the current circuit to the terminal if no filepath is specified
# we can pipe the output of qsyn to python to draw the circuit using qiskit
./qsyn -qc "qcir read $1; convert qcir zx; zx optimize; convert zx qcir; qcir optimize; qcir write; quit -f" | 
python3 -c "import sys; from qiskit import QuantumCircuit; qc = QuantumCircuit.from_qasm_str(sys.stdin.read()); qc.draw(output='latex', filename='$2')"