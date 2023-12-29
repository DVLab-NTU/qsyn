OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
rx(2.7938652739558347) q[1];
h q[0];
cx q[0],q[1];
y q[1];
x q[0];
