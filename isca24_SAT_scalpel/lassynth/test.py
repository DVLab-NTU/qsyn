# test_visualize.py
import json
from lassynth.lattice_surgery_synthesis import LatticeSurgerySolution

# load your dump
lasre = json.load(open("../../test.lasre.json"))

sol = LatticeSurgerySolution(lasre)

sol.to_3d_model_gltf(
    output_file_name="scene.gltf",
    stabilizer=-1,
    tube_len=2.0,
    attach_axes=True
)
print("wrote scene.gltf; open it in any glTF viewer")

sol.save_lasre("scene.lasre.json")
print("wrote scene.lasre.json")
