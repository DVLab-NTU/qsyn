OPENQASM 2.0;
include "qelib1.inc";
qreg q[60];

cx q[55], q[54];
rz(0.079303820273701781) q[54];
cx q[55], q[54];
