OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// Two RZ rotations with no intermediate Clifford collapse into one
// (MergeClass::same_pauli, sign = +1).
rz(pi/3) q[0];
rz(pi/4) q[0];
