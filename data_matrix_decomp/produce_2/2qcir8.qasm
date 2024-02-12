OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
cx q[1],q[0];
rz(1.9475953906135177) q[1];
h q[0];
ry(1.3753778407507358) q[1];
rx(4.5324525157586555) q[0];
