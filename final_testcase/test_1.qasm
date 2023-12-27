OPENQASM 2.0;
include "qelib1.inc";

qreg q[4];
creg c[4];
p(pi / 4) q[0];
z q[1];
s q[2];
h q[3];
ccx q[1], q[2], q[3];
cx q[3], q[0];
h q[3];
t q[1];
cx q[0], q[1];
cx q[1], q[2];
p(pi/4) q[0];
sdg q[2];
tdg q[1];
z q[1];
