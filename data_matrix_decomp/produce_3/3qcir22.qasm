OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
h q[2];
ry(6.174153619007306) q[0];
ry(5.646816893025301) q[1];
x q[2];
cx q[0],q[1];
y q[1];
cx q[2],q[0];
cx q[2],q[1];
rz(3.6564473143816443) q[0];
h q[0];
cx q[1],q[2];
cx q[0],q[2];
rz(0.43312314726154144) q[1];
x q[0];
x q[1];
x q[2];
rz(4.781608209861302) q[0];
cx q[2],q[1];
z q[0];
z q[1];
x q[2];
cx q[1],q[0];
z q[2];
cx q[2],q[0];
h q[1];
ry(0.34849692659874865) q[0];
cx q[2],q[1];
rx(0.6768508424315122) q[0];
x q[2];
x q[1];
cx q[1],q[2];
x q[0];
x q[1];
ry(5.152666594891793) q[0];
x q[2];
ry(3.338746536745297) q[0];
cx q[2],q[1];
x q[0];
h q[1];
x q[2];