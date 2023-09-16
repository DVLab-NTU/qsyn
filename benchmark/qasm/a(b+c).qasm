OPENQASM 2.0;
include "qelib1.inc";
qreg q[5];

x q[1];
x q[2];
ccx q[1], q[2], q[3];
x q[3];
ccx q[0], q[3], q[4];
x q[3];
ccx q[1], q[2], q[3];
x q[1];
x q[2];
