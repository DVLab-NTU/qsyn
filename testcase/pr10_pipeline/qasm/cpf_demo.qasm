OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
rz(0.7) q[0];
h q[0];
rz(0.3) q[0];
cx q[0], q[1];
rz(0.5) q[1];
