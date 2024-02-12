OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
cx q[0],q[1];
h q[1];
rz(4.407912716053098) q[0];
rz(4.748043975496299) q[0];
x q[1];
