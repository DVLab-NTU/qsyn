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
            parser.add_argument<size_t>("-k")
                .required(false)
                .default_value(3)
                .help("maximum cut size");
            parser.add_argument<std::string>("filepath")
                .constraint(path_readable)
                .help("path to the input dependency graph file");
        },
        [](ArgumentParser const& parser) {
            auto const max_cut_size = parser.get<size_t>("-k");
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
            std::ifstream ifs(filepath);
            test_pebble(P, ifs);
            return CmdExecResult::done;
        },
    };
}

Command qcir_oracle_cmd(QCirMgr& qcir_mgr) {
    return {
        "oracle",
        [](ArgumentParser& parser) {
            parser.description("synthesize a boolean oracle");

            parser.add_argument<size_t>("-a", "--n-ancilla")
                .required(false)
                .help("number of ancilla qubits to use")
                .default_value(0);

            parser.add_argument<std::string>("--xag")
                .required(true)
                .constraint(path_readable)
                .constraint(allowed_extension({".xaag"}))
                .help("path to the input xag file");

            parser.add_argument<size_t>("-k")
                .required(false)
                .default_value(3)
                .help("maximum cut size used in k-LUT partitioning");
        },
        [&](ArgumentParser const& parser) {
            auto n_ancilla = parser.get<size_t>("--n-ancilla");
            auto const k   = parser.get<size_t>("-k");
            auto* qcir     = qcir_mgr.add(qcir_mgr.get_next_id());
            std::ifstream ifs(parser.get<std::string>("--xag"));
            synthesize_boolean_oracle(ifs, qcir, n_ancilla, k);

            return CmdExecResult::done;
        },
    };
}

}  // namespace qsyn::qcir
