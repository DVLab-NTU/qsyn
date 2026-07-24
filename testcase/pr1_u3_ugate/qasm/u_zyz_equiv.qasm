OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// qubit 0: arbitrary u(theta, phi, lambda).
u(pi/3, pi/4, pi/5) q[0];

// qubit 1: the same unitary expanded via the QASM convention
//   U(theta, phi, lambda) = RZ(phi) * RY(theta) * RZ(lambda)
// which in left-to-right gate-stream order means rz(lambda) first.
rz(pi/5) q[1];
ry(pi/3) q[1];
rz(pi/4) q[1];
