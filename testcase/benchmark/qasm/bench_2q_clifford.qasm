OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
// Two RZ rotations on a qubit separated by a CX gate where it's the
// *control*: they should fuse via `cpf-merge` because RZ commutes with
// CX on the control wire.
rz(pi/4) q[0];
cx q[0],q[1];
rz(pi/4) q[0];
