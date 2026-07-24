OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// Two-qubit example mirroring the figure in the CPF paper:
//   rz(pi/8) q1 -- CX(0,1) -- rz(-pi/8) q1
//
// Z on the CX target is invariant under conjugation (CX z_target CX = z_target),
// so the two rotations should merge to zero. After cpf-global the tableau
// should have no Pauli rotations.
rz(pi/8) q[1];
cx q[0], q[1];
rz(-pi/8) q[1];
