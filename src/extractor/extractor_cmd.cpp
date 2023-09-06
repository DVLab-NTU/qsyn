/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define extractor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <memory>
#include <string>

#include "./extract.hpp"
#include "cli/cli.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_cmd.hpp"
#include "qcir/qcir_mgr.hpp"
#include "util/util.hpp"
#include "zx/zx_cmd.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_mgr.hpp"

using namespace std;
using namespace argparse;
extern ZXGraphMgr ZXGRAPH_MGR;
extern QCirMgr QCIR_MGR;

Command extraction_cmd();
Command extraction_settings_cmd();
Command extraction_print_cmd();
Command extraction_step_cmd();

bool add_extract_cmds() {
    if (!(CLI.add_command(extraction_cmd()) &&
          CLI.add_command(extraction_step_cmd()) &&
          CLI.add_command(extraction_settings_cmd()) &&
          CLI.add_command(extraction_print_cmd()))) {
        cerr << "Registering \"extract\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZX2QC
//----------------------------------------------------------------------

Command extraction_cmd() {
    return {"zx2qc",
            [](ArgumentParser &parser) {
                parser.description("extract QCir from ZXGraph");
            },
            [](ArgumentParser const &parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                if (!ZXGRAPH_MGR.get()->is_graph_like()) {
                    LOGGER.error("ZXGraph {0} is not graph-like. Not extractable!!", ZXGRAPH_MGR.focused_id());
                    return CmdExecResult::error;
                }
                size_t next_id = ZXGRAPH_MGR.get_next_id();
                ZXGRAPH_MGR.copy(next_id);
                Extractor ext(ZXGRAPH_MGR.get(), nullptr, nullopt);

                QCir *result = ext.extract();
                if (result != nullptr) {
                    QCIR_MGR.add(QCIR_MGR.get_next_id(), std::make_unique<QCir>(*result));
                    if (PERMUTE_QUBITS)
                        ZXGRAPH_MGR.remove(next_id);
                    else {
                        cout << "Note: the extracted circuit is up to a qubit permutation." << endl;
                        cout << "      Remaining permutation information is in ZXGraph id " << next_id << "." << endl;
                        ZXGRAPH_MGR.get()->add_procedure("ZX2QC");
                    }

                    QCIR_MGR.get()->add_procedures(ZXGRAPH_MGR.get()->get_procedures());
                    QCIR_MGR.get()->add_procedure("ZX2QC");
                    QCIR_MGR.get()->set_filename(ZXGRAPH_MGR.get()->get_filename());
                }

                return CmdExecResult::done;
            }};
}

Command extraction_step_cmd() {
    return {"extract",
            [](ArgumentParser &parser) {
                parser.description("perform step(s) in extraction");
                parser.add_argument<size_t>("-zxgraph")
                    .required(true)
                    .constraint(valid_zxgraph_id)
                    .metavar("ID")
                    .help("the ID of the ZXGraph to extract from");

                parser.add_argument<size_t>("-qcir")
                    .required(true)
                    .constraint(valid_qcir_id)
                    .metavar("ID")
                    .help("the ID of the QCir to extract to");

                auto mutex = parser.add_mutually_exclusive_group().required(true);

                mutex.add_argument<bool>("-cx")
                    .action(store_true)
                    .help("Extract CX gates");

                mutex.add_argument<bool>("-cz")
                    .action(store_true)
                    .help("Extract CZ gates");

                mutex.add_argument<bool>("-phase")
                    .action(store_true)
                    .help("Extract Z-rotation gates");

                mutex.add_argument<bool>("-H", "--hadamard")
                    .action(store_true)
                    .help("Extract Hadamard gates");

                mutex.add_argument<bool>("-clf", "--clear-frontier")
                    .action(store_true)
                    .help("Extract Z-rotation and then CZ gates");

                mutex.add_argument<bool>("-rmg", "--remove-gadgets")
                    .action(store_true)
                    .help("Remove phase gadgets in the neighbor of the frontiers");

                mutex.add_argument<bool>("-permute")
                    .action(store_true)
                    .help("Add swap gates to account for ZXGraph I/O permutations");

                mutex.add_argument<size_t>("-loop")
                    .nargs(NArgsOption::optional)
                    .metavar("N")
                    .help("Run N iteration of extraction loop. N is defaulted to 1");
            },
            [](ArgumentParser const &parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                ZXGRAPH_MGR.checkout(parser.get<size_t>("-zxgraph"));
                if (!ZXGRAPH_MGR.get()->is_graph_like()) {
                    LOGGER.error("ZXGraph {0} is not graph-like. Not extractable!!", ZXGRAPH_MGR.focused_id());
                    return CmdExecResult::error;
                }

                QCIR_MGR.checkout(parser.get<size_t>("-qcir"));

                if (ZXGRAPH_MGR.get()->get_num_outputs() != QCIR_MGR.get()->get_num_qubits()) {
                    cerr << "Error: number of outputs in graph is not equal to number of qubits in circuit" << endl;
                    return CmdExecResult::error;
                }

                Extractor ext(ZXGRAPH_MGR.get(), QCIR_MGR.get(), std::nullopt);

                if (parser.parsed("-loop")) {
                    ext.extraction_loop(parser.get<size_t>("-loop"));
                    return CmdExecResult::done;
                }

                if (parser.parsed("-clf")) {
                    ext.extract_singles();
                    ext.extract_czs(true);
                    return CmdExecResult::done;
                }

                if (parser.parsed("-phase")) {
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
                if (parser.parsed("-rmg")) {
                    if (ext.remove_gadget(true))
                        cout << "Gadget(s) are removed" << endl;
                    else
                        cout << "No gadget(s) are found" << endl;
                    return CmdExecResult::done;
                }

                if (parser.parsed("-permute")) {
                    ext.permute_qubits();
                    return CmdExecResult::done;
                }

                return CmdExecResult::error;  // should not reach
            }};
}

//----------------------------------------------------------------------
//    EXTPrint [ -Settings | -Frontier | -Neighbors | -Axels | -Matrix ]
//----------------------------------------------------------------------

Command extraction_print_cmd() {
    return {"extprint",
            [](ArgumentParser &parser) {
                parser.description("print info of extracting ZXGraph");

                auto mutex = parser.add_mutually_exclusive_group();

                mutex.add_argument<bool>("-settings")
                    .action(store_true)
                    .help("print the settings of extractor");
                mutex.add_argument<bool>("-frontier")
                    .action(store_true)
                    .help("print frontier of graph");
                mutex.add_argument<bool>("-neighbors")
                    .action(store_true)
                    .help("print neighbors of graph");
                mutex.add_argument<bool>("-axels")
                    .action(store_true)
                    .help("print axels of graph");
                mutex.add_argument<bool>("-matrix")
                    .action(store_true)
                    .help("print biadjancency");
            },
            [](ArgumentParser const &parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                if (parser.parsed("-settings") || parser.num_parsed_args() == 0) {
                    cout << endl;
                    cout << "Optimize Level:    " << OPTIMIZE_LEVEL << endl;
                    cout << "Sort Frontier:     " << (SORT_FRONTIER == true ? "true" : "false") << endl;
                    cout << "Sort Neighbors:    " << (SORT_NEIGHBORS == true ? "true" : "false") << endl;
                    cout << "Permute Qubits:    " << (PERMUTE_QUBITS == true ? "true" : "false") << endl;
                    cout << "Filter Duplicated: " << (FILTER_DUPLICATED_CXS == true ? "true" : "false") << endl;
                    cout << "Block Size:        " << BLOCK_SIZE << endl;
                } else {
                    if (!ZXGRAPH_MGR.get()->is_graph_like()) {
                        LOGGER.error("ZXGraph {0} is not graph-like. Not extractable!!", ZXGRAPH_MGR.focused_id());
                        return CmdExecResult::error;
                    }
                    Extractor ext(ZXGRAPH_MGR.get());
                    if (parser.parsed("-frontier")) {
                        ext.print_frontier();
                    } else if (parser.parsed("-neighbors")) {
                        ext.print_neighbors();
                    } else if (parser.parsed("-axels")) {
                        ext.print_axels();
                    } else if (parser.parsed("-matrix")) {
                        ext.create_matrix();
                        ext.print_matrix();
                    }
                }
                return CmdExecResult::done;
            }};
}

//------------------------------------------------------------------------------
//    EXTSet ...
//------------------------------------------------------------------------------

Command extraction_settings_cmd() {
    return {"extset",
            [](ArgumentParser &parser) {
                parser.description("set extractor parameters");
                parser.add_argument<size_t>("-optimize-level")
                    .choices({0, 1, 2, 3})
                    .help("optimization level");
                parser.add_argument<bool>("-permute-qubit")
                    .help("permute the qubit after extraction");
                parser.add_argument<size_t>("-block-size")
                    .help("Gaussian block size, only used in optimization level 0");
                parser.add_argument<bool>("-filter-cx")
                    .help("filter duplicated CXs");
                parser.add_argument<bool>("-frontier-sorted")
                    .help("sort frontier");
                parser.add_argument<bool>("-neighbors-sorted")
                    .help("sort neighbors");
            },
            [](ArgumentParser const &parser) {
                if (parser.parsed("-optimize-level")) {
                    OPTIMIZE_LEVEL = parser.get<size_t>("-optimize-level");
                }
                if (parser.parsed("-permute-qubit")) {
                    PERMUTE_QUBITS = parser.get<bool>("-permute-qubit");
                }
                if (parser.parsed("-block-size")) {
                    size_t block_size = parser.get<size_t>("-block-size");
                    if (block_size == 0)
                        cerr << "Error: block size value should > 0, skipping this option!!\n";
                    else
                        BLOCK_SIZE = block_size;
                }
                if (parser.parsed("-filter-cx")) {
                    FILTER_DUPLICATED_CXS = parser.get<bool>("-filter-cx");
                }
                if (parser.parsed("-frontier-sorted")) {
                    SORT_FRONTIER = parser.get<bool>("-frontier-sorted");
                }
                if (parser.parsed("-neighbors-sorted")) {
                    SORT_NEIGHBORS = parser.get<bool>("-neighbors-sorted");
                }
                return CmdExecResult::done;
            }};
}
