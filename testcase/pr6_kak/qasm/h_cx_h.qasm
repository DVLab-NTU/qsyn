OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];

// (H \otimes H) CX (H \otimes H) = CZ
// Resynthesising should give a CZ-equivalent circuit (KAK params a=0, b=0,
// c=pi/4 -- a pure ZZ rotation up to single-qubit phase).
h q[0];
h q[1];
cx q[0], q[1];
h q[0];
h q[1];
