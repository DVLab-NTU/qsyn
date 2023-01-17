for file in 0_qft_10 1_cm42a_207 2_cm82a_208 3_adr4_197 4_cycle10_2_110 5_ham15_107 6_dc2_222 7_sqn_258 8_inc_237 9_rd84_253 10_root_255; do
    ./qsyn < ref/230116-version-comparison/dof/${file}.dof > ref/230116-version-comparison/ref/${file}.log                                       
done
