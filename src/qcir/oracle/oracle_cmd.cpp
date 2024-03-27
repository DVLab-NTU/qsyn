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
#include "qcir/oracle/input.hpp"
#include "qcir/oracle/oracle.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

extern bool stop_requested();

namespace qsyn::qcir {

Command qcir_oracle_cmd(QCirMgr& qcir_mgr) {
    return {
        "oracle",
        [](ArgumentParser& parser) {
            parser.description("synthesize a boolean oracle");

            parser.add_argument<size_t>("-a", "--n-ancilla")
                .required(false)
                .help("number of ancilla qubits to use")
                .default_value(0);

            parser.add_argument<size_t>("-k")
                .required(false)
                .default_value(3)
                .help("maximum cut size used in k-LUT partitioning");

            auto mutex = parser.add_mutually_exclusive_group();

            mutex.add_argument<std::string>("--xag")
                .constraint(path_readable)
                .constraint(allowed_extension({".xaag"}))
                .help("path to the input xag file");

            mutex.add_argument<std::string>("--tt")
                .constraint(path_readable)
                .help("path to the input truth table file");

            mutex.add_argument<std::string>("-f", "--file")
                .constraint(path_readable)
                .constraint(allowed_extension({
                    ".aig",
                    ".baf",
                    ".bblif",
                    ".blif",
                    ".bench",
                    ".cnf",
                    ".dot",
                    ".edif",
                    ".eqn",
                    ".gml",
                    ".list",
                    ".mv",
                    ".pla",
                    ".smv",
                    ".v",
                }))
                .help("path to the input file, supported formats: aig, baf, bblif, blif, bench, cnf, dot, edif, eqn, gml, list, mv, pla, smv, v");

            parser.add_argument<bool>("-x")
                .action(store_true)
                .default_value(false)
                .help("use hex format for the truth table");

            parser.add_argument<std::string>("truth_table")
                .required(false)
                .help("truth table as a string");
        },
        [&](ArgumentParser const& parser) {
            auto n_ancilla = parser.get<size_t>("--n-ancilla");
            auto const k   = parser.get<size_t>("-k");

            XAG xag{};
            if (parser.parsed("--xag")) {
                std::ifstream ifs(parser.get<std::string>("--xag"));
                xag = from_xaag(ifs);
            } else {
                Abc_Ntk_t* ntk{};
                if (parser.parsed("--file")) {
                    auto file_name = parser.get<std::string>("--file");
                    ntk            = read_to_ntk(file_name);
                } else if (parser.parsed("--tt")) {
                    std::ifstream ifs(parser.get<std::string>("--tt"));
                    bool const hex = parser.get<bool>("-x");
                    ntk            = truth_table_to_ntk(ifs, hex);
                } else if (parser.parsed("truth_table")) {
                    auto input_string = parser.get<std::string>("truth_table");
                    std::istringstream input_stream(input_string);
                    ntk = truth_table_to_ntk(input_stream);
                } else {
                    spdlog::error("No input file or truth table is provided");
                    return CmdExecResult::error;
                }
                ntk = abc_resyn(ntk, true);
                xag = from_abc_ntk(ntk);
            }
            std::optional<QCir> qcir = synthesize_boolean_oracle(xag, n_ancilla, k);
            if (qcir.has_value()) {
                qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<QCir>(std::move(qcir.value())));
                if (parser.parsed("--xag")) {
                    qcir_mgr.get()->set_filename(parser.get<std::string>("--xag"));
                } else if (parser.parsed("--tt")) {
                    qcir_mgr.get()->set_filename(parser.get<std::string>("--tt"));
                }
            }

            return CmdExecResult::done;
        },
    };
}

}  // namespace qsyn::qcir
