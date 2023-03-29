import sys
sys.path.insert(0, "../pyzx")
import pyzx as zx

# circ = zx.Circuit.load("benchmark/benchmark_SABRE/large/sqn_258.qasm")
# g = circ.to_graph()
# with open("out-pyzx.zx", "w") as f:
#     f.write(g.to_zx())
g = zx.Graph.from_zx("qc2zx-qsyn.zx")

print(f"V: {g.num_vertices()}, E: {g.num_edges()}, T: {zx.tcount(g)}, I: {g.num_inputs()}, O: {g.num_outputs()}")

g1 = g.copy()
g = g.adjoint()
g.compose(g1)

print(f"V: {g.num_vertices()}, E: {g.num_edges()}, T: {zx.tcount(g)}, I: {g.num_inputs()}, O: {g.num_outputs()}")

# with open("out-qc2zx-qsyn.zx", "w") as f:
#     f.write(g.to_zx())

zx.simplify.full_reduce(g)

print(f"V: {g.num_vertices()}, E: {g.num_edges()}, T: {zx.tcount(g)}, I: {g.num_inputs()}, O: {g.num_outputs()}")

# with open("out-qc2zx-qsyn-reduced.zx", "w") as f:
#     f.write(g.to_zx())
