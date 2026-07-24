OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// Single-qubit U(theta=pi/3, phi=pi/4, lambda=pi/5).
// Re-synthesised, this should reduce to a single U3 (ZYZ) gate.
u(pi/3, pi/4, pi/5) q[0];
