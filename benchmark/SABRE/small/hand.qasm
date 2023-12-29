OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];


rz(1.5707963267948963) q[0]; 

cx q[0], q[1];

ry(-1.5707963267948963) q[1];

cx q[0], q[1];

ry(1.5707963267948963) q[1];
