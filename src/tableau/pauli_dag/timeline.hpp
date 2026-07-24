/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Flat Clifford / rotation timeline for staq-style folding. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <variant>
#include <vector>

#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

using TimelineEntry = std::variant<StabilizerTableau, PauliRotation>;

struct Timeline {
    std::size_t                 n_qubits = 0;
    std::vector<TimelineEntry> entries;
};

[[nodiscard]] Timeline build_timeline(Tableau const& tableau);
[[nodiscard]] Tableau  rebuild_tableau(Timeline const& timeline);

[[nodiscard]] std::size_t count_rotations(Timeline const& timeline);

}  // namespace qsyn::experimental::cpf::pauli_dag
