/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cstddef>
#include <ranges>
#include <unordered_set>
#include <vector>

#include "../qcir_mgr.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "qcir/oracle/deancilla.hpp"
#include "qcir/oracle/oracle.hpp"

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
            [&qcir_mgr](ArgumentParser const& parser) {
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

Command qcir_oracle_cmd(QCirMgr& qcir_mgr) {
    return {
        "oracle",
        [](ArgumentParser& parser) {
            parser.add_argument<size_t>("-i", "--n-input")
                .required(true)
                .help("number of input qubits to use");
            parser.add_argument<size_t>("-o", "--n-output")
                .required(true)
                .help("number of output qubits to use");
            parser.add_argument<size_t>("-a", "--n-ancilla")
                .required(false)
                .help("number of ancilla qubits to use")
                .default_value(0);

            parser.add_argument<size_t>("truth_table")
                .required(true)
                .nargs(NArgsOption::one_or_more)
                .help("truth table of the oracle");
        },
        [&](ArgumentParser const& parser) {
            auto n_input     = parser.get<size_t>("--n-input");
            auto n_output    = parser.get<size_t>("--n-output");
            auto n_ancilla   = parser.get<size_t>("--n-ancilla");
            auto truth_table = parser.get<std::vector<size_t>>("truth_table");

            spdlog::debug("oracle: n_input={}, n_output={}, n_ancilla={}", n_input, n_output, n_ancilla);

            if (truth_table.size() != (1 << n_input)) {
                spdlog::error("oracle: expected {} entries in the truth table, but got {} entries", (1 << n_input), truth_table.size());
                return CmdExecResult::error;
            }

            for (auto const i : std::views::iota(0ul, truth_table.size())) {
                spdlog::debug("oracle: truth_table[{}] = {}", i, truth_table[i]);
                if (truth_table[i] >= (1 << n_output)) {
                    spdlog::error("oracle: the {}-th entry in the truth table is {}, but the output is only {} bits", i, truth_table[i], n_output);
                    return CmdExecResult::error;
                }
            }

            synthesize_boolean_oracle(qcir_mgr.get(), n_ancilla, n_output, truth_table);

            return CmdExecResult::done;
        }};
}

}  // namespace qsyn::qcir
