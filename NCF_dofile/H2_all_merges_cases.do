// Enumerate NCF merge cases for H2 and dump the first four as QASM.
// Uses: tableau optimize ncf --all-merges --max-cases 12
// Writes: NCF_dofile/h2_case{0..3}.qasm
// Run from repo root: qsyn -f NCF_dofile/H2_all_merges_cases.do
qcir read h2_pyscf.qasm
convert qcir tableau
tableau optimize ncf --all-merges --max-cases 12

tableau checkout 0
convert tableau qcir -r ncf
qcir write NCF_dofile/h2_case0.qasm

tableau checkout 1
convert tableau qcir -r ncf
qcir write NCF_dofile/h2_case1.qasm

tableau checkout 2
convert tableau qcir -r ncf
qcir write NCF_dofile/h2_case2.qasm

tableau checkout 3
convert tableau qcir -r ncf
qcir write NCF_dofile/h2_case3.qasm
