OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];

// Classic GHZ-preparing circuit: H + 2x CX. Mostly Clifford, useful as a
// QSD sanity check (the resynthesised circuit should be unitarily equivalent
// to this one, even if the gate count balloons via the gray-code fallback).
h q[0];
cx q[0], q[1];
cx q[1], q[2];
