/****************************************************************************
  PackageName  [ qcir/deancilla ]
  Synopsis     [ Define functions for de-ancilla ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./deancilla.hpp"

#include <spdlog/spdlog.h>

namespace qsyn::qcir {

void deancilla(QCirMgr& qcir_mgr, size_t target_ancilla_count, std::vector<QubitIdType> const& ancilla_qubit_indexes) {
    auto qcir = qcir_mgr.get();
    for (auto const& qubit : qcir->get_qubits()) {
        spdlog::info("deancilla: qubit = {}", qubit->get_id());
    }
    spdlog::info("deancilla: target ancilla count = {}", target_ancilla_count);
    spdlog::info("deancilla: ancilla qubit indexes:");
    for (auto const& ancilla : ancilla_qubit_indexes) {
        spdlog::info("deancilla: ancilla qubit = {}", ancilla);
    }
}

}  // namespace qsyn::qcir
