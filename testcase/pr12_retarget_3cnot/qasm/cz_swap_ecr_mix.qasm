OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];

// Exercise non-CX 2-qubit gates plus arbitrary single-qubit rotations.
// After `qcir to-u3cx`, the output must contain only `u` and `cx`.
rx(0.4) q[0];
cz q[0], q[1];
ry(1.1) q[1];
swap q[1], q[2];
rz(0.7) q[2];
cx q[0], q[2];
