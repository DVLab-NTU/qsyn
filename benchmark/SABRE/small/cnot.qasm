OPENQASM 2.0;
include "qelib1.inc";

qreg q[4];

rz(pi/3) q[0];
ry(pi/5) q[1];
cx q[0], q[1];
rz(pi/7) q[1];
ry(pi/9) q[0];
rz(pi/2) q[2];
ry(pi/6) q[3];