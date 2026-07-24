OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
// Same topology as `u3_random.qasm` but the three angles are
// shifted by ~0.2 rad each.  After `qcir instantiate --target <id of
// u3_random>` the residual should fall back to <= 1e-7.
u(0.9,1.1,1.9) q[0];
