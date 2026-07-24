/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Explicit Pauli rotation DAG (dependency + merge edges). ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "tableau/pauli_rotation.hpp"

#include "./timeline.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

// Rotation node in the circuit-order frame: Pauli label after pushing past
// all Cliffords strictly before this gate in the timeline.
struct PauliDagNode {
    std::size_t   timeline_index = 0;
    PauliRotation rotation{{Pauli::i}, dvlab::Phase{0}};
    std::string   label_key;
};

// Directed acyclic graph: edge a -> b (a before b) means b's unitary depends on
// the relative order with a (cannot swap past a non-commuting region).
// `merge_pair` lists undirected pairs that staq `try_merge` would fuse.
struct PauliDagGraph {
    std::size_t                              n_qubits = 0;
    std::vector<PauliDagNode>                nodes;
    std::vector<std::pair<std::size_t, std::size_t>> dependency_edges;
    std::vector<std::pair<std::size_t, std::size_t>> merge_edges;
};

[[nodiscard]] PauliDagGraph build_pauli_dag_graph(Timeline const& timeline);

// Greedy layer merge on merge_edges: fuse connected components into one
// rotation per component at the earliest node, delete other nodes, return
// number of removed rotations.
[[nodiscard]] std::size_t graph_component_merge(Timeline& timeline, PauliDagGraph const& graph);

// Pick a maximum set of pairwise-disjoint merge edges (Cole-legal pairs that
// do not share endpoints), apply `fold_rotation_at` on later timeline indices
// first. For <=24 edges uses exact brute force; otherwise greedy by later index.
[[nodiscard]] std::size_t max_matching_merge(Timeline& timeline, PauliDagGraph const& graph);

}  // namespace qsyn::experimental::cpf::pauli_dag
