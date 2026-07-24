OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// 2-qubit CZ (3-CNOT-equivalent up to local Cliffords). Easy target for
// QSearch: any reasonable search should converge in <=3 layers.
h q[1];
cx q[0], q[1];
h q[1];
