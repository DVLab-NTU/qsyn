OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// X on the CX target is invariant: CX (I*X) CX = I*X.
// rx(pi/8) q[1] -- CX(0,1) -- rx(-pi/8) q[1] should cancel completely.
// After cpf-global, the tableau should contain no Pauli rotations.
rx(pi/8) q[1];
cx q[0], q[1];
rx(-pi/8) q[1];
