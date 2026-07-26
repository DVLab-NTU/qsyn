/**
 * @file ncf_convert.hpp
 * @brief Tableau ↔ NcfProgram ↔ QCir conversion and gate-cost helpers.
 *        CLI: ncf from-tableau, ncf to-qcir; also auto-built after tableau optimize ncf.
 */

#pragma once

#include <memory>
#include <optional>

#include "ncf/ncf_types.hpp"
#include "qcir/qcir.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental {

class NcfProgram;

[[nodiscard]] NcfGateCost gate_cost_from_tableau(
    Tableau const& tableau,
    std::string const& rotation_strategy = "ncf",
    std::string const& clifford_strategy = "hopt");

[[nodiscard]] std::optional<std::vector<size_t>> parse_original_indices(std::string const& label);

[[nodiscard]] std::unique_ptr<NcfProgram> build_ncf_program_from_tableau(
    Tableau const& post_ncf,
    size_t source_tableau_id,
    Tableau const* pre_ncf = nullptr);

[[nodiscard]] std::unique_ptr<Tableau> ncf_program_to_tableau(NcfProgram const& program);

[[nodiscard]] std::optional<qcir::QCir> ncf_program_to_qcir(
    NcfProgram const& program,
    std::string const& rotation_strategy = "ncf",
    std::string const& clifford_strategy = "hopt");

}  // namespace qsyn::experimental
