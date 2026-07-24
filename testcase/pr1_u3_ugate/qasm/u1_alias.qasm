OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// u1(lambda) is the Qiskit/QASM alias for p(lambda).
u1(pi/3) q[0];
