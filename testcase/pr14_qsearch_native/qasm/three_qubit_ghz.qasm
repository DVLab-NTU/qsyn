OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];

// 3-qubit GHZ-preparing circuit. After native QSearch via to-u3cx, the
// result must remain unitarily equivalent. Mostly Clifford so 2 CXs
// should suffice if the search finds the optimal decomposition.
h q[0];
cx q[0], q[1];
cx q[1], q[2];
