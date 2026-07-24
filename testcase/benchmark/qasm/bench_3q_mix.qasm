OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
// A 3-qubit mix of Clifford + continuous T-like rotations.  Used as a
// stress benchmark for `qcir cpf-optimize`; equivalence is the must-have
// property; gate count reduction is the nice-to-have.
h q[0];
rz(pi/8) q[0];
cx q[0],q[1];
rz(pi/8) q[1];
cx q[0],q[1];
rz(pi/8) q[0];
h q[0];
cx q[1],q[2];
rz(pi/8) q[2];
cx q[1],q[2];
