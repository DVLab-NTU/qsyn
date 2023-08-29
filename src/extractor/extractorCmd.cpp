/****************************************************************************
  FileName     [ extractorCmd.cpp ]
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
#include "qcir/qcirCmd.hpp"
#include "qcir/qcirMgr.hpp"
#include "util/util.hpp"
#include "zx/zxCmd.hpp"
#include "zx/zxGraph.hpp"
#include "zx/zxGraphMgr.hpp"

using namespace std;
using namespace ArgParse;
extern ZXGraphMgr zxGraphMgr;
extern QCirMgr qcirMgr;

Command ExtractCmd();
Command ExtractSetCmd();
Command ExtractPrintCmd();
Command ExtractStepCmd();

bool initExtractCmd() {
    if (!(cli.registerCommand("zx2qc", 5, ExtractCmd()) &&
          cli.registerCommand("extract", 4, ExtractStepCmd()) &&
          cli.registerCommand("extset", 4, ExtractSetCmd()) &&
          cli.registerCommand("extprint", 4, ExtractPrintCmd()))) {
        cerr << "Registering \"extract\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZX2QC
//----------------------------------------------------------------------

Command ExtractCmd() {
    return {"zx2qc",
            zxGraphMgrNotEmpty,
            [](ArgumentParser &parser) {
                parser.help("extract QCir from ZXGraph");
            },
            [](ArgumentParser const &parser) {
                if (!zxGraphMgr.get()->isGraphLike()) {
                    logger.error("ZXGraph {0} is not graph-like. Not extractable!!", zxGraphMgr.focusedID());
                    return CmdExecResult::ERROR;
                }
                size_t nextId = zxGraphMgr.getNextID();
                zxGraphMgr.copy(nextId);
                Extractor ext(zxGraphMgr.get(), nullptr, nullopt);

                QCir *result = ext.extract();
                if (result != nullptr) {
                    qcirMgr.add(qcirMgr.getNextID(), std::make_unique<QCir>(*result));
                    if (PERMUTE_QUBITS)
                        zxGraphMgr.remove(nextId);
                    else {
                        cout << "Note: the extracted circuit is up to a qubit permutation." << endl;
                        cout << "      Remaining permutation information is in ZXGraph id " << nextId << "." << endl;
                        zxGraphMgr.get()->addProcedure("ZX2QC");
                    }

                    qcirMgr.get()->addProcedures(zxGraphMgr.get()->getProcedures());
                    qcirMgr.get()->addProcedure("ZX2QC");
                    qcirMgr.get()->setFileName(zxGraphMgr.get()->getFileName());
                }

                return CmdExecResult::DONE;
            }};
}

Command ExtractStepCmd() {
    return {"extract",
            zxGraphMgrNotEmpty,
            [](ArgumentParser &parser) {
                parser.help("perform step(s) in extraction");
                parser.addArgument<size_t>("-zxgraph")
                    .required(true)
                    .constraint(validZXGraphId)
                    .metavar("ID")
                    .help("the ID of the ZXGraph to extract from");

                parser.addArgument<size_t>("-qcir")
                    .required(true)
                    .constraint(validQCirId)
                    .metavar("ID")
                    .help("the ID of the QCir to extract to");

                auto mutex = parser.addMutuallyExclusiveGroup().required(true);

                mutex.addArgument<bool>("-cx")
                    .action(storeTrue)
                    .help("Extract CX gates");

                mutex.addArgument<bool>("-cz")
                    .action(storeTrue)
                    .help("Extract CZ gates");

                mutex.addArgument<bool>("-phase")
                    .action(storeTrue)
                    .help("Extract Z-rotation gates");

                mutex.addArgument<bool>("-hadamard")
                    .action(storeTrue)
                    .help("Extract Hadamard gates");

                mutex.addArgument<bool>("-clfrontier")
                    .action(storeTrue)
                    .help("Extract Z-rotation and then CZ gates");

                mutex.addArgument<bool>("-rmgadgets")
                    .action(storeTrue)
                    .help("Remove phase gadgets in the neighbor of the frontiers");

                mutex.addArgument<bool>("-permute")
                    .action(storeTrue)
                    .help("Add swap gates to account for ZXGraph I/O permutations");

                mutex.addArgument<size_t>("-loop")
                    .nargs(NArgsOption::OPTIONAL)
                    .metavar("N")
                    .help("Run N iteration of extraction loop. N is defaulted to 1");
            },
            [](ArgumentParser const &parser) {
                zxGraphMgr.checkout(parser.get<size_t>("-zxgraph"));
                if (!zxGraphMgr.get()->isGraphLike()) {
                    logger.error("ZXGraph {0} is not graph-like. Not extractable!!", zxGraphMgr.focusedID());
                    return CmdExecResult::ERROR;
                }

                qcirMgr.checkout(parser.get<size_t>("-qcir"));

                if (zxGraphMgr.get()->getNumOutputs() != qcirMgr.get()->getNQubit()) {
                    cerr << "Error: number of outputs in graph is not equal to number of qubits in circuit" << endl;
                    return CmdExecResult::ERROR;
                }

                Extractor ext(zxGraphMgr.get(), qcirMgr.get(), std::nullopt);

                if (parser.parsed("-loop")) {
                    ext.extractionLoop(parser.get<size_t>("-loop"));
                    return CmdExecResult::DONE;
                }
                if (parser.parsed("-phase")) {
                    ext.extractSingles();
                    return CmdExecResult::DONE;
                }
                if (parser.parsed("-cz")) {
                    ext.extractCZs(true);
                    return CmdExecResult::DONE;
                }
                if (parser.parsed("-cx")) {
                    if (ext.biadjacencyElimination(true)) {
                        ext.updateGraphByMatrix();
                        ext.extractCXs();
                    }
                    return CmdExecResult::DONE;
                }
                if (parser.parsed("-hadamard")) {
                    ext.extractHsFromM2(true);
                    return CmdExecResult::DONE;
                }
                if (parser.parsed("-rmgadgets")) {
                    if (ext.removeGadget(true))
                        cout << "Gadget(s) are removed" << endl;
                    else
                        cout << "No gadget(s) are found" << endl;
                    return CmdExecResult::DONE;
                }

                if (parser.parsed("-permute")) {
                    ext.permuteQubit();
                    return CmdExecResult::DONE;
                }

                return CmdExecResult::ERROR;  // should not reach
            }};
}

//----------------------------------------------------------------------
//    EXTPrint [ -Settings | -Frontier | -Neighbors | -Axels | -Matrix ]
//----------------------------------------------------------------------

Command ExtractPrintCmd() {
    return {"extprint",
            zxGraphMgrNotEmpty,
            [](ArgumentParser &parser) {
                parser.help("print info of extracting ZXGraph");

                auto mutex = parser.addMutuallyExclusiveGroup();

                mutex.addArgument<bool>("-settings")
                    .action(storeTrue)
                    .help("print the settings of extractor");
                mutex.addArgument<bool>("-frontier")
                    .action(storeTrue)
                    .help("print frontier of graph");
                mutex.addArgument<bool>("-neighbors")
                    .action(storeTrue)
                    .help("print neighbors of graph");
                mutex.addArgument<bool>("-axels")
                    .action(storeTrue)
                    .help("print axels of graph");
                mutex.addArgument<bool>("-matrix")
                    .action(storeTrue)
                    .help("print biadjancency");
            },
            [](ArgumentParser const &parser) {
                if (parser.parsed("-settings") || parser.numParsedArguments() == 0) {
                    cout << endl;
                    cout << "Optimize Level:    " << OPTIMIZE_LEVEL << endl;
                    cout << "Sort Frontier:     " << (SORT_FRONTIER == true ? "true" : "false") << endl;
                    cout << "Sort Neighbors:    " << (SORT_NEIGHBORS == true ? "true" : "false") << endl;
                    cout << "Permute Qubits:    " << (PERMUTE_QUBITS == true ? "true" : "false") << endl;
                    cout << "Filter Duplicated: " << (FILTER_DUPLICATED_CXS == true ? "true" : "false") << endl;
                    cout << "Block Size:        " << BLOCK_SIZE << endl;
                } else {
                    if (!zxGraphMgr.get()->isGraphLike()) {
                        logger.error("ZXGraph {0} is not graph-like. Not extractable!!", zxGraphMgr.focusedID());
                        return CmdExecResult::ERROR;
                    }
                    Extractor ext(zxGraphMgr.get());
                    if (parser.parsed("-frontier")) {
                        ext.printFrontier();
                    } else if (parser.parsed("-neighbors")) {
                        ext.printNeighbors();
                    } else if (parser.parsed("-axels")) {
                        ext.printAxels();
                    } else if (parser.parsed("-matrix")) {
                        ext.createMatrix();
                        ext.printMatrix();
                    }
                }
                return CmdExecResult::DONE;
            }};
}

//------------------------------------------------------------------------------
//    EXTSet ...
//------------------------------------------------------------------------------

Command ExtractSetCmd() {
    return {"extset",
            [](ArgumentParser &parser) {
                parser.help("set extractor parameters");
                parser.addArgument<size_t>("-optimize-level")
                    .choices({0, 1, 2, 3})
                    .help("optimization level");
                parser.addArgument<bool>("-permute-qubit")
                    .help("permute the qubit after extraction");
                parser.addArgument<size_t>("-block-size")
                    .help("Gaussian block size, only used in optimization level 0");
                parser.addArgument<bool>("-filter-cx")
                    .help("filter duplicated CXs");
                parser.addArgument<bool>("-frontier-sorted")
                    .help("sort frontier");
                parser.addArgument<bool>("-neighbors-sorted")
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
                    size_t blockSize = parser.get<size_t>("-block-size");
                    if (blockSize == 0)
                        cerr << "Error: block size value should > 0, skipping this option!!\n";
                    else
                        BLOCK_SIZE = blockSize;
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
                return CmdExecResult::DONE;
            }};
}
