OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];

// A 3-qubit circuit with non-Clifford rotations sprinkled around CX gates.
// Exercises QSD's recursive path (currently scaffolded to fall through to
// the gray-code decomposer).
rx(pi/5) q[0];
ry(pi/7) q[1];
rz(pi/9) q[2];
cx q[0], q[1];
cx q[1], q[2];
rz(pi/11) q[0];
ry(pi/13) q[2];
cx q[0], q[2];
