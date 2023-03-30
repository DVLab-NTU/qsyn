import sys
sys.path.insert(0, "../pyzx")
import pyzx as zx

g = zx.Graph.from_zx("out-qsyn-before-bug.zx")

print("V: {}, E: {}, T: {}".format(g.num_vertices(), g.num_edges(), zx.tcount(g)))

zx.simplify.clifford_simp(g, quiet=False)
zx.simplify.gadget_simp(g, quiet=False)
zx.simplify.interior_clifford_simp(g, quiet=False)
zx.simplify.pivot_gadget_simp(g, quiet=False)

print("V: {}, E: {}, T: {}".format(g.num_vertices(), g.num_edges(), zx.tcount(g)))

with open("out-qsyn-pyzx-reduced.zx", "w") as f:
    f.write(g.to_zx())
