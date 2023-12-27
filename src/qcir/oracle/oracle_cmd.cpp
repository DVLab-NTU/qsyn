/****************************************************************************
  PackageName  [ qcir/oracle ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/oracle/oracle_cmd.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <fstream>
#include <ranges>
#include <vector>

#include "../qcir_mgr.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "qcir/oracle/k_lut.hpp"
#include "qcir/oracle/oracle.hpp"
#include "qcir/oracle/pebble.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

extern bool stop_requested();

namespace qsyn::qcir {

Command qcir_k_lut_cmd() {
    return {
        "k_lut",
        [](ArgumentParser& parser) {
            parser.description("perform quantum-aware k-LUT partitioning");
            parser.add_argument<size_t>("-c")
                .required(false)
                .default_value(3)
                .help("maximum cut size");
            parser.add_argument<std::string>("filepath")
                .constraint(path_readable)
                .help("path to the input dependency graph file");
        },
        [](ArgumentParser const& parser) {
            auto const max_cut_size = parser.get<size_t>("-c");
            auto const filepath     = parser.get<std::string>("filepath");

            std::ifstream ifs(filepath);

            test_k_lut_partition(max_cut_size, ifs);
            return CmdExecResult::done;
        },
    };
}

Command qcir_pebble_cmd() {
    return {
        "pebble",
        [](ArgumentParser& parser) {
            parser.description("test ancilla qubit scheduling with SAT based reversible pebbling game");
            parser.add_argument<size_t>("-p")
                .required(true)
                .help("number of ancilla qubits to use");
            parser.add_argument<std::string>("filepath")
                .constraint(path_readable)
                .help("path to the input dependency graph file");
        },
        [](ArgumentParser const& parser) {
            auto const P        = parser.get<size_t>("-p");
            auto const filepath = parser.get<std::string>("filepath");
            test_pebble(P, filepath);
            return CmdExecResult::done;
        },
    };
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

            auto* qcir = qcir_mgr.add(qcir_mgr.get_next_id());
            synthesize_boolean_oracle(qcir, n_ancilla, n_output, truth_table);

            return CmdExecResult::done;
        },
    };
}

}  // namespace qsyn::qcir
