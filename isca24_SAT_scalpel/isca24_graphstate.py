import json
import time
from lassynth import LatticeSurgerySynthesizer

KISSAT_DIR = "/usr/local/bin/"  # change this to yours


def graph_state_lassynth(edges,
                         n_q: int = 8,
                         starting_depth: int = 3,
                         index=-1,
                         wdir=""):

    data = {
        "max_i":
        n_q,
        "max_j":
        2,
        "max_k":
        starting_depth,
        "ports": [{
            "location": [i, 0, starting_depth],
            "direction": "-K",
            "z_basis_direction": "I",
        } for i in range(n_q)]
    }

    # stabilizers are X on qubit and Z on all its neighbors
    data["stabilizers"] = []
    for i in range(n_q):
        stab = "." * i + "X" + "." * (n_q - i - 1)
        for edge in edges:
            if edge[0] == i:
                stab = stab[:edge[1]] + "Z" + stab[edge[1] + 1:]
            if edge[1] == i:
                stab = stab[:edge[0]] + "Z" + stab[edge[0] + 1:]
        data["stabilizers"].append(stab)

    solver = LatticeSurgerySynthesizer(solver="kissat",
                                                kissat_dir=KISSAT_DIR)
    result = solver.optimize_depth(
        specification=data,
        start_depth=starting_depth,
        print_detail=False, # set to true for more information
        # dimacs_file_name_prefix=wdir + str(index), # uncomment to save files
        # sat_log_file_name_prefix=wdir + str(index),
        )
    # result.save_lasre(wdir + str(index) + ".lasre.json") # uncomment for file
    return result.get_depth()


def run_opt_exp(wdir: str = "./results/"): # change working directory up to you
    with open(wdir + "opt_stats", "w") as file:
        file.write("id, num_edge, opt_depth, runtime\n")

    for s in range(101):
        # read the edges from the graph library
        with open("isca24_graphs.json", "r") as file:
            graphs = json.load(file)
        edges = graphs["8"][s]
        start_time = time.time()
        depth = graph_state_lassynth(edges, index=s, wdir=wdir)
        runtime = time.time() - start_time
        with open(wdir + "isca24_graph_results.csv", "a") as file:
            file.write(f"{s}, {len(edges)}, {depth}, {runtime}\n")


if __name__ == "__main__":
    run_opt_exp()
