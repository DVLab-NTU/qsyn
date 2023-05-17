FOLDER=230417-extractionPhyV2
mkdir -p ref-phy/${FOLDER}
##NOTE - Full Reduced Stats
for file in cm82a_208 adr4_197 cm42a_207 cycle10_2_110 ham15_107 dc2_222 inc_237 rd84_253 sqn_258 root_255; do
    ../../qsyn -f "dof-phy/${file}.dof" > ref-phy/${FOLDER}/${file}-O3.log 2>&1
done
##NOTE - Other 1
for file in urf2_277 life_238 9symml_195 dist_223 urf1_278; do
    ../../qsyn -f "dof-phy/${file}.dof" > ref-phy/${FOLDER}/${file}-O3.log 2>&1
done
##NOTE - Other 2
for file in cm85a_209 mlp4_245 square_root_7 pm1_249 sqrt8_260 z4_268 rd73_252 rd53_251; do
    ../../qsyn -f "dof-phy/${file}.dof" > ref-phy/${FOLDER}/${file}-O3.log 2>&1
done
##NOTE - Big cases
for file in hwb8_113 urf1_149; do
    ../../qsyn -f "dof-phy/${file}.dof" > ref-phy/${FOLDER}/${file}-O3.log 2>&1
done
# ##NOTE - Likely to be floating point issue
# for file in qft_n20; do
#     ../../qsyn -f "dof-phy/${file}.dof" > ref-phy/${FOLDER}/${file}-O3.log 2>&1
# done