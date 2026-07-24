OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// All three Cartan invariants non-zero -- a worst-case 2-qubit input for KAK.
// With `--three-cnot-kak` the resulting circuit should have <= 3 CX gates.
rx(0.4) q[0];
ry(0.7) q[1];
cx q[0], q[1];
rz(0.3) q[0];
rz(0.5) q[1];
cx q[1], q[0];
ry(0.2) q[1];
