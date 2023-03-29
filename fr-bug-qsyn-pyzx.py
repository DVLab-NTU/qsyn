import sys
sys.path.insert(0, "../pyzx")
import pyzx as zx

g = zx.Graph.from_zx("out-qsyn-qsyn.zx")

print("V: {}, E: {}, T: {}".format(g.num_vertices(), g.num_edges(), zx.tcount(g)))

zx.simplify.full_reduce(g)

print("V: {}, E: {}, T: {}".format(g.num_vertices(), g.num_edges(), zx.tcount(g)))

with open("out-qsyn-pyzx-reduced.zx", "w") as f:
    f.write(g.to_zx())
