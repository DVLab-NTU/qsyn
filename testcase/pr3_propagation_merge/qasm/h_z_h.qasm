OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// rz(a); h; h; rz(b)  -- the two h's cancel into identity, so propagation
// across the intermediate maps Z to Z; the two rotations merge as
// MergeClass::propagation_aligned with sign = +1.
rz(pi/5) q[0];
h q[0];
h q[0];
rz(pi/6) q[0];
