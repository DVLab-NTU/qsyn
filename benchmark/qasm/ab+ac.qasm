OPENQASM 2.0;
include "qelib1.inc";
qreg q[6];

ccx q[0], q[1], q[3];
ccx q[0], q[2], q[4];
x q[3];
x q[4];
ccx q[3], q[4], q[5];
x q[3];
x q[5];
x q[4];
ccx q[0], q[2], q[4];
ccx q[0], q[1], q[3];
