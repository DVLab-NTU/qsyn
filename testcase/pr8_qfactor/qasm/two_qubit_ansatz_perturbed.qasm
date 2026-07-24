OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
// Same gate sequence (= same ansatz topology) as the target circuit,
// but every U3 angle is perturbed.  QFactor-lite should tune the six
// U3 gates so the unitary matches the target up to tolerance.
u(0.7,0.5,0.9) q[0];
u(0.4,0.4,0.6) q[1];
cx q[0],q[1];
u(0.6,0.3,1.0) q[1];
cx q[0],q[1];
u(0.4,0.7,0.5) q[0];
