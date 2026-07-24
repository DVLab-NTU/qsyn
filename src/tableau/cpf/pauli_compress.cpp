/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Pauli-rotation compression (merge / cancel / Clifford-snap) ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./pauli_compress.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "./angle_utils.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau_optimization.hpp"
#include "util/phase.hpp"

namespace qsyn::experimental::cpf {

Tableau from_pauli_list(size_t n_qubits,
                        std::vector<std::pair<std::string, double>> const& rotations) {
    std::vector<PauliRotation> rots;
    rots.reserve(rotations.size());
    for (auto const& [pauli_str, angle] : rotations) {
        // Tighten the rationalisation tolerance well below any compression
        // budget so the canonical form is faithful to the input angle.
        rots.emplace_back(std::string_view{pauli_str}, dvlab::Phase(angle, 1e-9));
    }

    if (rots.empty()) {
        return Tableau{n_qubits};
    }
    return Tableau{StabilizerTableau{n_qubits}, std::move(rots)};
}

namespace {

// Locate the (first) Clifford prefix and the (first) Pauli-rotation block of a
// collapsed tableau, which is in the canonical `[Clifford | PauliRotation[]]`
// form.
std::pair<StabilizerTableau*, std::vector<PauliRotation>*>
locate_clifford_and_rotations(Tableau& tableau) {
    StabilizerTableau* clifford          = nullptr;
    std::vector<PauliRotation>* rotations = nullptr;
    for (auto& sub : tableau) {
        if (auto* st = std::get_if<StabilizerTableau>(&sub); st != nullptr && clifford == nullptr) {
            clifford = st;
        } else if (auto* rot = std::get_if<std::vector<PauliRotation>>(&sub); rot != nullptr && rotations == nullptr) {
            rotations = rot;
        }
    }
    return {clifford, rotations};
}

// Round `phase` to the nearest integer multiple of pi/2 (i.e. the nearest
// Clifford angle). Returns the nearest Clifford phase together with the L2
// "coefficient" cost of the snap, defined consistently with
// compress_ncf.py: cost = |theta - theta_nearest| / 2.
std::pair<dvlab::Phase, double> nearest_clifford(dvlab::Phase const& phase) {
    auto const rad = phase_to_radians(phase);            // (-pi, pi]
    auto const x   = rad / std::numbers::pi;             // (-1, 1] in units of pi
    auto const k   = static_cast<dvlab::Phase::IntegralType>(std::llround(2.0 * x));
    dvlab::Phase const nearest(k, 2);                    // k * pi/2, normalised mod 2pi
    auto const cost = std::abs(rad - phase_to_radians(nearest)) / 2.0;
    return {nearest, cost};
}

}  // namespace

PauliCompressStats pauli_compress(Tableau& tableau, PauliCompressOptions const& opts) {
    PauliCompressStats stats;

    // 1) canonicalise to [Clifford | PauliRotation[]]
    collapse(tableau);

    auto [clifford, rotations] = locate_clifford_and_rotations(tableau);
    if (rotations == nullptr) {
        return stats;
    }
    stats.n_before = rotations->size();

    // 2) D-merge: sum phases of rotations sharing the same Pauli string.
    //    PauliRotation normalises the sign into the phase, so the Pauli letters
    //    alone form a canonical key.
    {
        std::unordered_map<std::string, size_t> first_index;
        std::vector<PauliRotation> merged;
        merged.reserve(rotations->size());
        for (auto const& rot : *rotations) {
            auto key = rot.pauli_product().to_string('+');
            if (auto it = first_index.find(key); it != first_index.end()) {
                merged[it->second].phase() += rot.phase();
            } else {
                first_index.emplace(std::move(key), merged.size());
                merged.push_back(rot);
            }
        }
        if (merged.size() < rotations->size()) {
            stats.n_merged = rotations->size() - merged.size();
        }
        *rotations = std::move(merged);
    }

    // 3) F-cancel: drop rotations whose accumulated phase is zero.
    {
        auto const before = rotations->size();
        std::erase_if(*rotations, [](PauliRotation const& rot) {
            return is_zero_phase(rot.phase());
        });
        stats.n_cancelled = before - rotations->size();
    }

    // 4) HYBRID: greedily snap rotations to their nearest Clifford angle,
    //    cheapest first, while the cumulative L2 cost stays within budget.
    if (opts.l2_budget > 0.0) {
        struct SnapCandidate {
            size_t index;
            double cost;
            dvlab::Phase target;
            bool to_zero;
        };
        std::vector<SnapCandidate> candidates;
        candidates.reserve(rotations->size());
        for (size_t i = 0; i < rotations->size(); ++i) {
            auto const& rot = (*rotations)[i];
            if (is_clifford_phase(rot.phase())) continue;  // already Clifford
            auto const [target, cost] = nearest_clifford(rot.phase());
            candidates.push_back({i, cost, target, is_zero_phase(target)});
        }
        std::ranges::sort(candidates, [](auto const& a, auto const& b) { return a.cost < b.cost; });

        double sumsq                 = 0.0;
        auto const budget_sq         = opts.l2_budget * opts.l2_budget;
        std::vector<bool> drop(rotations->size(), false);
        for (auto const& cand : candidates) {
            auto const next_sumsq = sumsq + cand.cost * cand.cost;
            if (next_sumsq > budget_sq) break;
            sumsq = next_sumsq;
            if (cand.to_zero) {
                drop[cand.index] = true;
                ++stats.n_snapped_zero;
            } else {
                (*rotations)[cand.index].phase() = cand.target;
                ++stats.n_snapped_clifford;
            }
        }
        stats.l2_used = std::sqrt(sumsq);

        std::vector<PauliRotation> kept;
        kept.reserve(rotations->size());
        for (size_t i = 0; i < rotations->size(); ++i) {
            if (!drop[i]) kept.push_back((*rotations)[i]);
        }
        *rotations = std::move(kept);
    }

    // 5) Fold rotations snapped to a non-zero Clifford angle into the leading
    //    Clifford segment (they are now Clifford gates, not T-like rotations).
    if (opts.absorb_clifford && clifford != nullptr && stats.n_snapped_clifford > 0) {
        absorb_clifford_rotations(*clifford, *rotations);
        remove_identities(*rotations);
    }

    stats.n_after = rotations->size();
    return stats;
}

}  // namespace qsyn::experimental::cpf
