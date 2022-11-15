for file in qft_10 cm82a_208 adr4_197 cm42a_207 cycle10_2_110 dc2_222 ham15_107 root_255; do
    ./qsyn -f "ref/${file}.dof" > ref/log/${file}-O3.log 2>&1
done