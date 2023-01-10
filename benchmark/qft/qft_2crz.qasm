OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
rz(pi/4) q[1];
crz(pi/2) q[1],q[0];
h q[1];
