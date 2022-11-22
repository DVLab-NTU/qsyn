for file in qft_10 cm82a_208 adr4_197 cm42a_207 cycle10_2_110 ham15_107 dc2_222 inc_237 rd84_253 sqn_258 root_255; do
    ./qsyn -f "ref/${file}.dof" > ref/221122/${file}-O3.log 2>&1
done