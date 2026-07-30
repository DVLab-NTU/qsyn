tableau from-terms /home/chenying/qsyn/out/ncf2q_10cases/OH-JW10/OH-JW10.json --use-coeff
tableau optimize collapse
tableau copy 1
tableau checkout 1
convert tableau qcir -r naive
qcir write /home/chenying/qsyn/out/ncf2q_10cases/OH-JW10/OH-JW10_before_2qncf_clifford_rz.qasm
tableau checkout 0
tableau optimize ncf --two-qubit --window 128
ncf print --blocks
convert tableau qcir -r ncf
qcir write /home/chenying/qsyn/out/ncf2q_10cases/OH-JW10/OH-JW10_after_2qncf_clifford_rz.qasm
