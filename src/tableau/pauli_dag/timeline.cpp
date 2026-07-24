/****************************************************************************
  PackageName  [ tableau / pauli_dag ]
  Synopsis     [ Timeline import/export. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./timeline.hpp"

#include "tableau/cpf/angle_utils.hpp"

namespace qsyn::experimental::cpf::pauli_dag {

Timeline build_timeline(Tableau const& tableau) {
    Timeline tl;
    tl.n_qubits = tableau.n_qubits();
    for (auto const& sub : tableau) {
        if (auto const* st = std::get_if<StabilizerTableau>(&sub)) {
            tl.entries.emplace_back(*st);
            continue;
        }
        auto const* rots = std::get_if<std::vector<PauliRotation>>(&sub);
        if (rots == nullptr) continue;
        for (auto const& r : *rots) {
            if (is_zero_phase(r.phase())) continue;
            tl.entries.emplace_back(r);
        }
    }
    return tl;
}

Tableau rebuild_tableau(Timeline const& timeline) {
    Tableau tab{timeline.n_qubits};
    std::vector<PauliRotation> cur_rots;
    auto                       flush_rots = [&]() {
        if (!cur_rots.empty()) {
            tab.emplace_back(std::move(cur_rots));
            cur_rots.clear();
        }
    };

    for (auto const& e : timeline.entries) {
        if (auto const* st = std::get_if<StabilizerTableau>(&e)) {
            flush_rots();
            tab.emplace_back(*st);
            continue;
        }
        auto const* r = std::get_if<PauliRotation>(&e);
        if (r != nullptr) {
            if (is_zero_phase(r->phase())) continue;
            cur_rots.push_back(*r);
        }
    }
    flush_rots();
    return tab;
}

std::size_t count_rotations(Timeline const& timeline) {
    std::size_t n = 0;
    for (auto const& e : timeline.entries) {
        if (std::holds_alternative<PauliRotation>(e)) ++n;
    }
    return n;
}

}  // namespace qsyn::experimental::cpf::pauli_dag
