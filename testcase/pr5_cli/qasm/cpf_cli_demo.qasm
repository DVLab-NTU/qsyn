OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
// A small Clifford-T-style circuit that exercises all three CPF
// CLI sub-commands.  The two RZ pieces on q[0] are separated by an
// H sandwich (handled by `cpf-global`); the two RX pieces on q[1]
// are separated by a CX gate (handled by `cpf-merge`).
rz(pi/4) q[0];
h        q[0];
rz(pi/4) q[0];
rx(pi/5) q[1];
cx q[0],q[1];
rx(pi/5) q[1];
