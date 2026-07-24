OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// "Random" 2-qubit unitary with all three Cartan invariants nonzero and
// no easy structure to exploit. Used by PR-B (LBFGS minimiser) to verify
// that the 3-CNOT QFactor ansatz can match it.
u3(0.4, 0.7, 1.1) q[0];
u3(0.8, 0.3, 2.4) q[1];
cx q[0], q[1];
u3(0.5, 0.2, 1.7) q[0];
u3(1.3, 0.9, 0.6) q[1];
cx q[1], q[0];
u3(0.7, 0.4, 1.0) q[0];
u3(0.2, 1.1, 0.3) q[1];
cx q[0], q[1];
u3(0.6, 0.5, 0.8) q[0];
u3(0.9, 0.2, 1.4) q[1];
