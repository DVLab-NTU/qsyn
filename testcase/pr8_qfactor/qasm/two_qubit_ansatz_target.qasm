OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
// A 2-qubit "target" circuit with non-trivial entanglement.
u(0.5,0.3,0.7) q[0];
u(0.6,0.2,0.4) q[1];
cx q[0],q[1];
u(0.4,0.1,0.8) q[1];
cx q[0],q[1];
u(0.2,0.9,0.3) q[0];
