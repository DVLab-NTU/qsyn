// Default single-path NCF on the H2 (PySCF) circuit.
// Flow: qcir -> tableau -> optimize ncf -> qcir (-r ncf) -> h2_ncf.qasm
// Run from repo root: qsyn -f NCF_dofile/H2.do
qcir read h2_pyscf.qasm
convert qcir tableau
tableau print -c
tableau checkout 0
tableau copy 1
tableau checkout 1
tableau optimize ncf
convert tableau qcir -r ncf


qcir checkout 1
qcir print -d

tableau checkout 1
tableau print -c
qcir write h2_ncf.qasm
