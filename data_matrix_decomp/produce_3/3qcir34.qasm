OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
x q[2];
y q[0];
h q[1];
rz(0.5429323554707581) q[2];
h q[1];
ry(5.268218049882132) q[0];
cx q[1],q[2];
ry(1.1902443364417064) q[0];
cx q[0],q[1];
h q[2];
rz(4.711591393805955) q[1];
rz(2.512365843367074) q[2];
x q[0];
rx(2.9306269718437163) q[0];
h q[2];
y q[1];
rz(2.1415241077879124) q[0];
rz(6.110628075687143) q[1];
rx(0.5835005882531406) q[2];
cx q[0],q[1];
ry(3.3873318990849506) q[2];
cx q[2],q[0];
x q[1];
y q[2];
cx q[0],q[1];
cx q[0],q[1];
x q[2];
cx q[0],q[2];
rx(1.407029709293741) q[1];
cx q[2],q[0];
y q[1];