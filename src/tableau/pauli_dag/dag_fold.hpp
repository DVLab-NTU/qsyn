/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Full Pauli-DAG CPF fold: staq + graph + global prune. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

#include "./pauli_dag.hpp"
#include "./staq_fold.hpp"
#include "tableau/cpf/global_fold.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental::cpf {

struct GraphFoldStats {
    std::size_t n_nodes              = 0;
    std::size_t n_merge_edges        = 0;
    std::size_t n_component_merges   = 0;
};

struct DagFoldStats {
    pauli_dag::StaqFoldStats        staq{};
    GraphFoldStats                  graph{};
    pauli_dag::GlobalPruneStats     global{};
    GlobalFoldStats                 local{};
    std::size_t                     n_passes = 0;
};

struct DagFoldOptions {
    bool run_staq             = true;
    bool run_graph_merge      = true;
    bool use_matching_merge   = false;
    bool run_global_prune     = true;
    bool run_global_fold      = true;
    std::size_t max_passes    = 8;
};

// Full pipeline on a Tableau:
//   1. Flatten to timeline
//   2. staq `fold_forward` sweeps (primary phase folding)
//   3. Build explicit Pauli DAG, merge (greedy edge or max matching)
//   4. Optional zero-label global prune
//   5. `global_fold` fixpoint on interleaved tableau
DagFoldStats dag_fold(Tableau& tableau, DagFoldOptions const& opt = {});

}  // namespace qsyn::experimental::cpf
