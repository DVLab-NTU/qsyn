OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// rz(a) q[1]; x q[0]; rz(b) q[1]   -- the X gate on a *different* qubit
// does not touch q[1]'s Z basis, so the two RZ rotations on q[1] merge as
// MergeClass::propagation_aligned.
//
// For a real sign-flip example we use:
//   rx(a) q[0]; z q[0]; rx(b) q[0];
// Here `Z * X * Z = -X`, so the propagated P_L is -X, matching -P_R, and
// the merge falls into MergeClass::propagation_negated with sign = -1
// (i.e. new phase = b - a on q[0]).
rx(pi/5) q[0];
z q[0];
rx(pi/6) q[0];
