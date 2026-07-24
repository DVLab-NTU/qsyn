OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
// A 2-qubit phase-fold candidate: two RZ rotations on the control are
// separated by an H sandwich that does *not* cancel them, but a single
// pair of identical RX rotations on the target sandwiched by a CX gate
// should fuse via propagation_merge.
//
// After `qcir cpf-optimize` we expect:
//   - the two RX(...) on q[1] to fuse into a single RX
//   - the RZ rotations on q[0] to fuse via the H ladder
rz(pi/7) q[0];
h        q[0];
rz(pi/9) q[0];
rx(pi/5) q[1];
cx q[0],q[1];
rx(pi/5) q[1];
