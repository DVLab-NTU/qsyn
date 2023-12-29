OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];

rz(pi/3) q[0];
ry(pi/5) q[1];
cx q[0], q[1];