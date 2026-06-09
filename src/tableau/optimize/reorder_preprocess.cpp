#include "../tableau_optimization.hpp"

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace qsyn::experimental {
namespace {

void validate_pr_phases(std::vector<PauliRotation> const& pr) {
    for (size_t col = 0; col < pr.size(); ++col) {
        auto const& rotation = pr[col];
        if (rotation.is_CZ()) {
            spdlog::error(
                "reorder_preprocess: export error: column {} is CZ-marked; expected pi/4, +/-pi/2, or pi phases only",
                col);
            throw std::logic_error(
                "reorder_preprocess: export error: CZ-marked PR column is not supported");
        }
        auto const& phase = rotation.phase();
        bool const ok =
            phase == dvlab::Phase(1, 4) || phase == dvlab::Phase(-1, 4) ||
            phase == dvlab::Phase(1, 2) || phase == dvlab::Phase(-1, 2) ||
            phase == dvlab::Phase(1) || phase == dvlab::Phase(-1);
        if (!ok) {
            spdlog::error(
                "reorder_preprocess: export error: column {} has unsupported phase {}; expected pi/4, +/-pi/2, or pi",
                col,
                phase);
            throw std::logic_error(
                "reorder_preprocess: export error: unsupported PR phase");
        }
    }
}

}  // namespace

void reorder_preprocess(Tableau& tableau) {
    auto const pr_idx_opt = find_unified_pr_block_index(tableau);
    if (!pr_idx_opt.has_value()) {
        spdlog::warn("reorder_preprocess: no unified PR block; skipping");
        return;
    }
    auto* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[*pr_idx_opt]);
    if (pr_vec == nullptr || pr_vec->empty()) {
        spdlog::warn("reorder_preprocess: empty PR block; skipping");
        return;
    }

    validate_pr_phases(*pr_vec);
}

}  // namespace qsyn::experimental
