OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
cx q[0],q[2];
ry(3.161763195575918) q[1];
cx q[1],q[0];
h q[2];
