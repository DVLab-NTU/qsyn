OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];

ccx q[0],q[1],q[2];
ccx q[0],q[1],q[2];