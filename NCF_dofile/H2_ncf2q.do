// Two-qubit NCF (paper §IV-A2) on H2 JW STO-3G.
// Grading + Table III + sliding window w=128.
// Emits Clifford+RZ before and after; does NOT synthesize 2q blocks to Clifford+T.
//
// Run from repo root:
//   ./build/qsyn -f NCF_dofile/H2_ncf2q.do

// ---- load Pauli terms ----
tableau from-terms tests/ncf/data/h2_jw_sto3g.json --use-coeff
tableau optimize collapse

// ---- BEFORE: naive Clifford+RZ (each Pauli as its own Z-gadget) ----
tableau copy 1
tableau checkout 1
convert tableau qcir -r naive
qcir write NCF_dofile/h2_before_2qncf_clifford_rz.qasm

// ---- AFTER: 2-qubit NCF → Clifford+RZ (no Clifford+T synth) ----
tableau checkout 0
tableau optimize ncf --two-qubit --window 128
ncf print --summary
ncf print --blocks
convert tableau qcir -r ncf
qcir write NCF_dofile/h2_after_2qncf_clifford_rz.qasm
