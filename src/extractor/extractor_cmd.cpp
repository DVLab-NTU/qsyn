/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define extractor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <cstddef>
#include <string>

#include "./extract.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_cmd.hpp"
#include "qcir/qcir_mgr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_mgr.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

using qsyn::qcir::QCirMgr;
using qsyn::zx::ZXGraphMgr;

namespace qsyn::extractor {

dvlab::Command extraction_step_cmd(zx::ZXGraphMgr& zxgraph_mgr, QCirMgr& qcir_mgr) {
    return {"step",
            [&](ArgumentParser& parser) {
                parser.description("perform step(s) in extraction");
                parser.add_argument<size_t>("-zx", "--zxgraph")
                    .required(true)
                    .constraint(dvlab::utils::valid_mgr_id(zxgraph_mgr))
                    .metavar("ID")
                    .help("the ID of the ZXGraph to extract from");

                parser.add_argument<size_t>("-qc", "--qcir")
                    .required(true)
                    .constraint(valid_qcir_id(qcir_mgr))
                    .metavar("ID")
                    .help("the ID of the QCir to extract to");

                auto mutex = parser.add_mutually_exclusive_group().required(true);

                mutex.add_argument<bool>("-cx")
                    .action(store_true)
                    .help("Extract CX gates");

                mutex.add_argument<bool>("-cz")
                    .action(store_true)
                    .help("Extract CZ gates");

                mutex.add_argument<bool>("-ph", "--phase")
                    .action(store_true)
                    .help("Extract Z-rotation gates");

                mutex.add_argument<bool>("-H", "--hadamard")
                    .action(store_true)
                    .help("Extract Hadamard gates");

                mutex.add_argument<bool>("--clear-frontier")
                    .action(store_true)
                    .help("Extract Z-rotation and then CZ gates");

                mutex.add_argument<bool>("--remove-gadgets")
                    .action(store_true)
                    .help("Remove phase gadgets in the neighbor of the frontiers");

                mutex.add_argument<bool>("--permute")
                    .action(store_true)
                    .help("Add swap gates to account for ZXGraph I/O permutations");

                mutex.add_argument<size_t>("-l", "--loop")
                    .nargs(NArgsOption::optional)
                    .default_value(1)
                    .metavar("N")
                    .help("Run N iteration of extraction loop. N is defaulted to 1");
            },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return CmdExecResult::error;
                auto zx_id   = parser.get<size_t>("--zxgraph");
                auto qcir_id = parser.get<size_t>("--qcir");
                if (!zxgraph_mgr.find_by_id(zx_id)->is_graph_like()) {
                    spdlog::error("ZXGraph {} is not extractable because it is not graph-like!!", zx_id);
                    return CmdExecResult::error;
                }

                if (zxgraph_mgr.find_by_id(zx_id)->get_num_outputs() != qcir_mgr.find_by_id(qcir_id)->get_num_qubits()) {
                    spdlog::error("Number of outputs in ZXGraph {} is not equal to number of qubits in QCir {}!!", zx_id, qcir_id);
                    return CmdExecResult::error;
                }

                zxgraph_mgr.checkout(zx_id);
                qcir_mgr.checkout(qcir_id);
                Extractor ext(zxgraph_mgr.get(), qcir_mgr.get() /*, std::nullopt */);

                if (parser.parsed("--loop")) {
                    ext.extraction_loop(parser.get<size_t>("--loop"));
                    return CmdExecResult::done;
                }

                if (parser.parsed("--clear-frontier")) {
                    ext.clean_frontier();
                    return CmdExecResult::done;
                }

                if (parser.parsed("--phase")) {
                    ext.extract_singles();
                    return CmdExecResult::done;
                }
                if (parser.parsed("-cz")) {
                    ext.extract_czs(true);
                    return CmdExecResult::done;
                }
                if (parser.parsed("-cx")) {
                    if (ext.biadjacency_eliminations(true)) {
                        ext.update_graph_by_matrix();
                        ext.extract_cxs();
                    }
                    return CmdExecResult::done;
                }
                if (parser.parsed("-H")) {
                    ext.extract_hadamards_from_matrix(true);
                    return CmdExecResult::done;
                }
                if (parser.parsed("--remove-gadgets")) {
                    if (ext.remove_gadget(true)) {
                        spdlog::info("Gadget(s) are removed");
                    } else {
                        spdlog::info("No gadgets are found");
                    }
                    return CmdExecResult::done;
                }

                if (parser.parsed("--permute")) {
                    ext.permute_qubits();
                    return CmdExecResult::done;
                }

                return CmdExecResult::error;  // should not reach
            }};
}

dvlab::Command extraction_print_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"print",
            [](ArgumentParser& parser) {
                parser.description("print the info pertinent to extraction for the focused ZXGraph");

                auto mutex = parser.add_mutually_exclusive_group().required(true);

                mutex.add_argument<bool>("-f", "--frontier")
                    .action(store_true)
                    .help("print frontier of graph");
                mutex.add_argument<bool>("-n", "--neighbors")
                    .action(store_true)
                    .help("print neighbors of graph");
                mutex.add_argument<bool>("-a", "--axels")
                    .action(store_true)
                    .help("print axels of graph");
                mutex.add_argument<bool>("-m", "--matrix")
                    .action(store_true)
                    .help("print biadjancency");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr.get()->is_graph_like()) {
                    spdlog::error("ZXGraph {} is not extractable because it is not graph-like!!", zxgraph_mgr.focused_id());
                    return CmdExecResult::error;
                }
                Extractor ext(zxgraph_mgr.get(), false);
                if (parser.parsed("--frontier")) {
                    ext.print_frontier();
                } else if (parser.parsed("--neighbors")) {
                    ext.print_neighbors();
                } else if (parser.parsed("--axels")) {
                    ext.print_axels();
                } else if (parser.parsed("--matrix")) {
                    ext.update_matrix();
                    ext.print_matrix();
                }
                return CmdExecResult::done;
            }};
}

Command extractor_config_cmd() {
    return {"config",
            [](ArgumentParser& parser) {
                parser.description("configure the behavior of extractor");
                parser.add_argument<size_t>("--optimize-level")
                    .choices({0, 1, 2, 3})
                    .help("optimization level");
                parser.add_argument<bool>("--permute-qubit")
                    .help("permute the qubit after extraction");
                parser.add_argument<size_t>("--block-size")
                    .help("Gaussian block size, only used in optimization level 0");
                parser.add_argument<bool>("--filter-cx")
                    .help("filter duplicated CXs");
                parser.add_argument<bool>("--reduce-cz")
                    .help("optimize CZs by row eliminations");
                parser.add_argument<bool>("--frontier-sorted")
                    .help("sort frontier");
                parser.add_argument<bool>("--neighbors-sorted")
                    .help("sort neighbors");
                parser.add_argument<bool>("--gadget-first")
                    .help("remove gadget before cleaning CZs");
                parser.add_argument<float>("--cx-coefficient")
                    .help("coefficient of cx");
            },
            [](ArgumentParser const& parser) {
                auto print_current_config = true;
                if (parser.parsed("--optimize-level")) {
                    OPTIMIZE_LEVEL       = parser.get<size_t>("--optimize-level");
                    print_current_config = false;
                }
                if (parser.parsed("--permute-qubit")) {
                    PERMUTE_QUBITS       = parser.get<bool>("--permute-qubit");
                    print_current_config = false;
                }
                if (parser.parsed("--block-size")) {
                    auto block_size = parser.get<size_t>("--block-size");
                    if (block_size > 0) {
                        BLOCK_SIZE = parser.get<size_t>("--block-size");
                    } else {
                        spdlog::warn("Block size should be a positive number!!");
                        spdlog::warn("Ignoring this option...");
                    }
                    print_current_config = false;
                }
                if (parser.parsed("--filter-cx")) {
                    FILTER_DUPLICATE_CXS = parser.get<bool>("--filter-cx");
                    print_current_config = false;
                }
                if (parser.parsed("--reduce-cz")) {
                    REDUCE_CZS           = parser.get<bool>("--reduce-cz");
                    print_current_config = false;
                }
                if (parser.parsed("--frontier-sorted")) {
                    SORT_FRONTIER        = parser.get<bool>("--frontier-sorted");
                    print_current_config = false;
                }
                if (parser.parsed("--neighbors-sorted")) {
                    SORT_NEIGHBORS       = parser.get<bool>("--neighbors-sorted");
                    print_current_config = false;
                }
                if (parser.parsed("--gadget-first")) {
                    GADGET_FIRST         = parser.get<bool>("--gadget-first");
                    print_current_config = false;
                }
                if (parser.parsed("--cx-coefficient")) {
                    CX_COEFF       = parser.get<float>("--cx-coefficient");
                    print_current_config = false;
                }
                // if no option is specified, print the current settings
                if (print_current_config) {
                    fmt::println("");
                    fmt::println("Optimize Level:        {}", OPTIMIZE_LEVEL);
                    fmt::println("Sort Frontier:         {}", SORT_FRONTIER);
                    fmt::println("Sort Neighbors:        {}", SORT_NEIGHBORS);
                    fmt::println("Permute Qubits:        {}", PERMUTE_QUBITS);
                    fmt::println("Filter Duplicated CXs: {}", FILTER_DUPLICATE_CXS);
                    fmt::println("Reduce CZs:            {}", REDUCE_CZS);
                    fmt::println("Block Size:            {}", BLOCK_SIZE);
                    fmt::println("Rm. gadget before CZs: {}", GADGET_FIRST);
                    fmt::println("CX Coefficient:        {}", CX_COEFF);
                }
                return CmdExecResult::done;
            }};
}

Command extract_cmd(zx::ZXGraphMgr& zxgraph_mgr, qcir::QCirMgr& qcir_mgr) {
    auto cmd = Command{"extract",
                       [](ArgumentParser& parser) {
                           parser.description("extract ZXGraph to QCir");
                           parser.add_subparsers("extractor-cmd").required(true);
                       },
                       [&](ArgumentParser const& /* unused */) {
                           return CmdExecResult::error;
                       }};

    cmd.add_subcommand("extractor-cmd", extractor_config_cmd());
    cmd.add_subcommand("extractor-cmd", extraction_step_cmd(zxgraph_mgr, qcir_mgr));
    cmd.add_subcommand("extractor-cmd", extraction_print_cmd(zxgraph_mgr));

    return cmd;
}

bool add_extract_cmds(dvlab::CommandLineInterface& cli, zx::ZXGraphMgr& zxgraph_mgr, qcir::QCirMgr& qcir_mgr) {
    if (!cli.add_command(extract_cmd(zxgraph_mgr, qcir_mgr))) {
        spdlog::error("Registering \"extract\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn::extractor
