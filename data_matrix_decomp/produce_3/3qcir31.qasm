OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
rz(4.125601200288638) q[0];
cx q[1],q[2];
cx q[2],q[0];
rx(0.49112199736881923) q[1];
ry(1.169656776907587) q[2];
cx q[1],q[0];
x q[2];
y q[1];
h q[0];
cx q[0],q[2];
rx(1.1157870708624855) q[1];
x q[0];
cx q[1],q[2];
y q[2];
cx q[1],q[0];
cx q[2],q[1];
h q[0];
cx q[1],q[2];
z q[0];
cx q[0],q[1];
z q[2];
ry(2.6978933774677794) q[0];
rz(2.75283959616416) q[2];
x q[1];
x q[2];
cx q[0],q[1];
z q[2];
rz(0.9461481437904139) q[0];
x q[1];
ry(2.73206137604579) q[0];
ry(3.924703804085658) q[1];
z q[2];
ry(4.016083203377509) q[0];
rz(5.194813650410731) q[2];
rz(3.781309363527462) q[1];
z q[0];
rz(3.420823465329159) q[2];
y q[1];
cx q[2],q[0];
h q[1];
cx q[1],q[0];
z q[2];