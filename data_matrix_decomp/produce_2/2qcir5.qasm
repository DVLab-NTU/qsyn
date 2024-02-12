OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
x q[0];
rx(0.3867943008162528) q[1];
cx q[0],q[1];
cx q[0],q[1];
