OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// CX (Z_control) CX = Z_control -- both rotations are equal Pauli, just
// merged to one rz(pi/4) on q[0] after cpf-global.
rz(pi/8) q[0];
cx q[0], q[1];
rz(pi/8) q[0];
