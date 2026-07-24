OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
// A mix of Clifford (H, S, CX) and non-Clifford (T, Rz(pi/8)) gates.
// `qcir cpf-optimize` should leave behaviour invariant while reducing
// the Pauli-rotation count via propagation_merge + full_optimize.
h  q[0];
t  q[0];
cx q[0],q[1];
rz(pi/8) q[1];
cx q[0],q[1];
t  q[1];
h  q[1];
cx q[1],q[2];
rz(pi/8) q[2];
cx q[1],q[2];
s  q[2];
