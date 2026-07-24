OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// u2(phi, lambda) = U(pi/2, phi, lambda)
u2(pi/4, pi/8) q[0];
