import sys
sys.path.insert(0, "../pyzx")
import pyzx as zx

# circ = zx.Circuit.load("benchmark/benchmark_SABRE/large/sqn_258.qasm")
# g = circ.to_graph()
# print(f"V: {g.num_vertices()}, E: {g.num_edges()}, T: {zx.tcount(g)}, I: {g.num_inputs()}, O: {g.num_outputs()}")


# g1 = g.copy()
# g1 = g1.adjoint()
# g.compose(g1)

g = zx.Graph.from_zx("out-qsyn-interc.zx")

print(f"V: {g.num_vertices()}, E: {g.num_edges()}, T: {zx.tcount(g)}, I: {g.num_inputs()}, O: {g.num_outputs()}")

# with open("out-pyzx-pyzx.zx", "w") as f:
#     f.write(g.to_zx())

zx.simplify.full_reduce(g, quiet=False)

print(f"V: {g.num_vertices()}, E: {g.num_edges()}, T: {zx.tcount(g)}, I: {g.num_inputs()}, O: {g.num_outputs()}")

# with open("out-pyzx-pyzx-reduced.zx", "w") as f:
#     f.write(g.to_zx())
