/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cstddef>
#include <ranges>
#include <vector>

#include "../qcir_mgr.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "qcir/oracle/oracle.hpp"
#include "qcir/oracle/pebble.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

extern bool stop_requested();

namespace qsyn::qcir {

Command qcir_pebble_cmd() {
    return {"pebble",
            [](ArgumentParser& parser) {
                parser.description("test ancilla qubit scheduling with SAT based reversible pebbling game");
            },
            [](ArgumentParser const& /*parser*/) {
                test_pebble();
                return CmdExecResult::done;
            }};
}

Command qcir_oracle_cmd(QCirMgr& qcir_mgr) {
    return {
        "oracle",
        [](ArgumentParser& parser) {
            parser.description("synthesize a boolean oracle");

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

            auto new_id = qcir_mgr.get_next_id();
            qcir_mgr.add(new_id);
            qcir_mgr.checkout(new_id);
            synthesize_boolean_oracle(qcir_mgr.get(), n_ancilla, n_output, truth_table);

            return CmdExecResult::done;
        }};
}

}  // namespace qsyn::qcir
