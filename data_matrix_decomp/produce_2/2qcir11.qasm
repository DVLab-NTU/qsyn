OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
cx q[1],q[0];
x q[1];
z q[0];
rz(0.5557254515809313) q[0];
rx(0.6263549464921244) q[1];
cx q[0],q[1];
cx q[0],q[1];
cx q[0],q[1];
x q[1];
rz(2.9327088646577177) q[0];
cx q[0],q[1];