OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];

// rx(pi/5) -- H -- rz(pi/7) -- H -- rx(pi/9)
//
// trace_replay keeps these as
//     [Stab_id, [X(pi/5)], Stab_H, [Z(pi/7)], Stab_H, [X(pi/9)]]
//
// cpf-global should:
//   - propagate Z(pi/7) through the second H -> X(pi/7), merge with the
//     third rotation block. The block between the two H's becomes empty.
//   - prune empty blocks + coalesce the two H Stabs -> identity, leaving
//     one rotation block [X(pi/5 + pi/7 + pi/9) = X(143*pi/315)].
rx(pi/5) q[0];
h q[0];
rz(pi/7) q[0];
h q[0];
rx(pi/9) q[0];
