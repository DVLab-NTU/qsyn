OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
// "easy" benchmark: every rotation merges trivially.
rz(pi/8) q[0];
rz(pi/8) q[0];
rx(pi/6) q[1];
rx(pi/6) q[1];
