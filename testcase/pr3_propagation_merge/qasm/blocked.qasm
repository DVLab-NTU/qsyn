OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// rz(a); h; ry(b);  -- after propagating Z through H we get X, which does
// NOT match Y, so this segment falls into MergeClass::blocked and the two
// rotations remain.
rz(pi/3) q[0];
h q[0];
ry(pi/4) q[0];
