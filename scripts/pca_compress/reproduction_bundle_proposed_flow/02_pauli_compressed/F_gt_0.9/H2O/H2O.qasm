OPENQASM 2.0;
include "qelib1.inc";
qreg q[14];

rz(1.5707963267948966) q[0];
cx q[7], q[3];
cx q[6], q[3];
cx q[5], q[3];
rz(1.5707963267948966) q[3];
cx q[5], q[3];
cx q[6], q[3];
cx q[7], q[3];
