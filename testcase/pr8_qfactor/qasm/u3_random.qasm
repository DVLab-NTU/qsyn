OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
// Single arbitrary U3 -- the QFactor-lite polish should
// drive the residual essentially to zero, since the
// target is the gate itself.
u(0.7,1.3,2.1) q[0];
