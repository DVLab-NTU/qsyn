/****************************************************************************
  PackageName  [ qcir/deancilla ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cstddef>
#include <unordered_set>
#include <vector>

#include "../qcir_mgr.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "qcir/oracle/deancilla.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

extern bool stop_requested();

namespace qsyn::qcir {

Command qcir_deancilla_cmd(QCirMgr& qcir_mgr) {
    return {"deancilla",
            [](ArgumentParser& parser) {
                parser.description("create a new circuit that uses less ancilla qubits");

                parser.add_argument<size_t>("-n", "--n-ancilla")
                    .required(true)
                    .help("target ancilla qubits after optimization");
                parser.add_argument<QubitIdType>("-a", "--ancilla")
                    .required(true)
                    .nargs(NArgsOption::one_or_more)
                    .help("ancilla qubit ids, information stored in these qubits may not be preserved");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;

                auto target_ancilla_count = parser.get<size_t>("--n-ancilla");
                auto ancilla_qubits_ids   = parser.get<std::vector<QubitIdType>>("--ancilla");

                if (ancilla_qubits_ids.size() < target_ancilla_count) {
                    spdlog::error("deancilla: target ancilla count is larger than the number of ancilla qubits");
                    return CmdExecResult::error;
                } else if (ancilla_qubits_ids.size() == target_ancilla_count) {
                    spdlog::info("deancilla: target ancilla count is equal to the number of ancilla qubits, nothing to do");
                    return CmdExecResult::done;
                }

                auto qubits       = qcir_mgr.get()->get_qubits();
                auto qubit_id_set = std::unordered_set<QubitIdType>();

                for (auto const& qubit : qubits) {
                    qubit_id_set.insert(qubit->get_id());
                }
                for (auto const& ancilla : ancilla_qubits_ids) {
                    if (qubit_id_set.find(ancilla) == qubit_id_set.end()) {
                        spdlog::error("deancilla: ancilla qubit {} does not exist", ancilla);
                        return CmdExecResult::error;
                    }
                }

                deancilla(qcir_mgr, target_ancilla_count, ancilla_qubits_ids);

                return CmdExecResult::done;
            }};
}

}  // namespace qsyn::qcir
