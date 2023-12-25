import numpy as np
import sys
import os
import random
import argparse




from qiskit.circuit import Gate, QuantumCircuit, QuantumRegister
from qiskit.circuit.library import (
    CXGate,
    HGate,
    SGate,
    TGate,
    TdgGate,
)


def my_random_circuit(num_qubits, num_gates, seed=None):
    """Generate a pseudo random Clifford circuit."""

    clifford = ["cx", "h", "s"]
    cnot_t   = ["cx", "t", "tdg"]
    cnot_t_h = ["cx", "t", "tdg", "h"]

    instructions = {
        "cx": (CXGate(), 2),
        "s": (SGate(), 1),
        "h": (HGate(), 1),
        "t": (TGate(), 1),
        "tdg": (TdgGate(), 1)}

    if isinstance(seed, np.random.Generator):
        rng = seed
    else:
        rng = np.random.default_rng(seed)


    samples = rng.choice(cnot_t, num_gates)


    circ = QuantumCircuit(num_qubits)

    for name in samples:
        gate, nqargs = instructions[name]
        qargs = rng.choice(range(num_qubits), nqargs, replace=False).tolist()
        circ.append(gate, qargs)

    return circ


if __name__ == "__main__":
    
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", "--number_qubit",type=int, help="qubit number" , default=3)
    parser.add_argument("-g", "--gate_number" ,type=int, help="gate number"  , default=50)
    parser.add_argument("-o", "--output"        ,help="output path", default="./example.qasm")
    args = parser.parse_args()

    n = args.number_qubit
    g = args.gate_number
    outputdir = args.output
        
    qc = my_random_circuit(num_qubits=n, num_gates=g)
    qc.qasm(filename=outputdir)





