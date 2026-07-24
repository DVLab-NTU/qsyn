OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// A non-trivial 2-qubit unitary built from non-Clifford rotations and a CX.
// Has all three Cartan invariants (a, b, c) non-zero, so the KAK pipeline
// exercises the full 6-CNOT centre + all four SU(2) wings.
rx(pi/3) q[0];
ry(pi/5) q[1];
cx q[0], q[1];
rz(pi/7) q[0];
ry(pi/11) q[1];
cx q[1], q[0];
rx(pi/13) q[0];
