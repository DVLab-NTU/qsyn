OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// All angles are integer multiples of pi/2 -> Clifford unitary.
u(pi/2, pi/2, pi/2) q[0];
u(pi, 0, pi) q[0];
