OPENQASM 2.0;
include "qelib1.inc";
qreg q[5];

x q[1];
x q[2];
mcx q[3] q[0] q[2] q[1];
x q[1];
x q[2];
