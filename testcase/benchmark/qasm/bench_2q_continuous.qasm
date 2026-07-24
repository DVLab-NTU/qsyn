OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
// Continuous-angle pair (sum = 7/15 pi, not a 2pi/k angle for small k):
// this is the regime where TODD-based phasepoly cannot fuse the
// rotations but CPF can.
rz(pi/3) q[0];
cx q[0],q[1];
rz(2*pi/5) q[0];
cx q[0],q[1];
