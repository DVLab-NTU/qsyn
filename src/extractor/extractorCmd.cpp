/****************************************************************************
  FileName     [ extractorCmd.cpp ]
  PackageName  [ extractor ]
  Synopsis     [ Define extractor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iostream>
#include <string>

#include "cmdParser.h"
#include "deviceMgr.h"
#include "extract.h"
#include "qcir.h"
#include "qcirCmd.h"
#include "qcirMgr.h"
#include "util.h"
#include "zxCmd.h"
#include "zxGraph.h"
#include "zxGraphMgr.h"

using namespace std;
using namespace ArgParse;
extern size_t verbose;
extern int effLimit;
extern ZXGraphMgr zxGraphMgr;
extern QCirMgr *qcirMgr;
extern DeviceMgr *deviceMgr;

unique_ptr<ArgParseCmdType> ExtractCmd();
unique_ptr<ArgParseCmdType> ExtractSetCmd();
unique_ptr<ArgParseCmdType> ExtractPrintCmd();
unique_ptr<ArgParseCmdType> ExtractStepCmd();

bool initExtractCmd() {
    if (!(cmdMgr->regCmd("ZX2QC", 5, ExtractCmd()) &&
          cmdMgr->regCmd("EXTRact", 4, ExtractStepCmd()) &&
          cmdMgr->regCmd("EXTSet", 4, ExtractSetCmd()) &&
          cmdMgr->regCmd("EXTPrint", 4, ExtractPrintCmd()))) {
        cerr << "Registering \"extract\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZX2QC
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> ExtractCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZX2QC");

    cmd->precondition = []() { return zxGraphMgrNotEmpty("ZX2QC"); };

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("extract QCir from ZXGraph");
    };

    cmd->onParseSuccess = [](mythread::stop_token st, ArgumentParser const &parser) {
        if (!zxGraphMgr.get()->isGraphLike()) {
            cerr << "Error: ZXGraph (id: " << zxGraphMgr.get()->getId() << ") is not graph-like. Not extractable!!" << endl;
            return CMD_EXEC_ERROR;
        }
        size_t nextId = zxGraphMgr.getNextID();
        zxGraphMgr.copy(nextId);
        Extractor ext(zxGraphMgr.get(), nullptr, nullopt, st);

        QCir *result = ext.extract();
        if (result != nullptr) {
            qcirMgr->addQCir(qcirMgr->getNextID());
            qcirMgr->setQCircuit(result);
            if (PERMUTE_QUBITS)
                zxGraphMgr.remove(nextId);
            else {
                cout << "Note: the extracted circuit is up to a qubit permutation." << endl;
                cout << "      Remaining permutation information is in ZXGraph id " << nextId << "." << endl;
                zxGraphMgr.get()->addProcedure("ZX2QC");
            }

            qcirMgr->getQCircuit()->addProcedures(zxGraphMgr.get()->getProcedures());
            qcirMgr->getQCircuit()->setFileName(zxGraphMgr.get()->getFileName());
        }

        return CMD_EXEC_DONE;
    };
    return cmd;
}

unique_ptr<ArgParseCmdType> ExtractStepCmd() {
    auto cmd = make_unique<ArgParseCmdType>("EXTRact");

    cmd->precondition = []() { return zxGraphMgrNotEmpty("EXTRact"); };

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("perform step(s) in extraction");
        parser.addArgument<size_t>("-ZXgraph")
            .required(true)
            .constraint(validZXGraphId)
            .metavar("ID")
            .help("the ID of the ZXGraph to extract from");

        parser.addArgument<size_t>("-QCir")
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
    };

    cmd->onParseSuccess = [](mythread::stop_token st, ArgumentParser const &parser) {
        zxGraphMgr.checkout(parser["-zxgraph"]);
        if (!zxGraphMgr.get()->isGraphLike()) {
            cerr << "Error: ZXGraph (id: " << zxGraphMgr.get()->getId() << ") is not graph-like. Not extractable!!" << endl;
            return CMD_EXEC_ERROR;
        }

        qcirMgr->checkout2QCir(parser["-qcir"]);

        if (zxGraphMgr.get()->getNumOutputs() != qcirMgr->getQCircuit()->getNQubit()) {
            cerr << "Error: number of outputs in graph is not equal to number of qubits in circuit" << endl;
            return CMD_EXEC_ERROR;
        }

        Extractor ext(zxGraphMgr.get(), qcirMgr->getQCircuit(), std::nullopt, st);

        if (parser["-loop"].isParsed()) {
            ext.extractionLoop(parser["-loop"]);
            return CMD_EXEC_DONE;
        }
        if (parser["-phase"].isParsed()) {
            ext.extractSingles();
            return CMD_EXEC_DONE;
        }
        if (parser["-cz"].isParsed()) {
            ext.extractCZs(true);
            return CMD_EXEC_DONE;
        }
        if (parser["-cx"].isParsed()) {
            if (ext.biadjacencyElimination(true)) {
                ext.updateGraphByMatrix();
                ext.extractCXs();
            }
            return CMD_EXEC_DONE;
        }
        if (parser["-hadamard"].isParsed()) {
            ext.extractHsFromM2(true);
            return CMD_EXEC_DONE;
        }
        if (parser["-rmgadgets"].isParsed()) {
            if (ext.removeGadget(true))
                cout << "Gadget(s) are removed" << endl;
            else
                cout << "No gadget(s) are found" << endl;
            return CMD_EXEC_DONE;
        }

        if (parser["-permute"].isParsed()) {
            ext.permuteQubit();
            return CMD_EXEC_DONE;
        }

        return CMD_EXEC_ERROR;  // should not reach
    };

    return cmd;
}

//----------------------------------------------------------------------
//    EXTPrint [ -Settings | -Frontier | -Neighbors | -Axels | -Matrix ]
//----------------------------------------------------------------------

unique_ptr<ArgParseCmdType> ExtractPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("EXTPrint");

    cmd->precondition = []() { return zxGraphMgrNotEmpty("EXTPrint"); };

    cmd->parserDefinition = [](ArgumentParser &parser) {
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
    };

    cmd->onParseSuccess = [](mythread::stop_token st, ArgumentParser const &parser) {
        if (parser["-settings"].isParsed() || parser.numParsedArguments() == 0) {
            cout << endl;
            cout << "Optimize Level:    " << OPTIMIZE_LEVEL << endl;
            cout << "Sort Frontier:     " << (SORT_FRONTIER == true ? "true" : "false") << endl;
            cout << "Sort Neighbors:    " << (SORT_NEIGHBORS == true ? "true" : "false") << endl;
            cout << "Permute Qubits:    " << (PERMUTE_QUBITS == true ? "true" : "false") << endl;
            cout << "Filter Duplicated: " << (FILTER_DUPLICATED_CXS == true ? "true" : "false") << endl;
            cout << "Block Size:        " << BLOCK_SIZE << endl;
        } else {
            if (!zxGraphMgr.get()->isGraphLike()) {
                cerr << "Error: ZXGraph (id: " << zxGraphMgr.get()->getId() << ") is not graph-like. Not extractable!!" << endl;
                return CMD_EXEC_ERROR;
            }
            Extractor ext(zxGraphMgr.get());
            if (parser["-frontier"].isParsed()) {
                ext.printFrontier();
            } else if (parser["-neighbors"].isParsed()) {
                ext.printNeighbors();
            } else if (parser["-axels"].isParsed()) {
                ext.printAxels();
            } else if (parser["-matrix"].isParsed()) {
                ext.createMatrix();
                ext.printMatrix();
            }
        }

        return CMD_EXEC_DONE;
    };
    return cmd;
}

//------------------------------------------------------------------------------
//    EXTSet ...
//------------------------------------------------------------------------------

unique_ptr<ArgParseCmdType> ExtractSetCmd() {
    auto cmd = make_unique<ArgParseCmdType>("EXTSet");
    cmd->parserDefinition = [](ArgumentParser &parser) {
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
    };

    cmd->onParseSuccess = [](mythread::stop_token st, ArgumentParser const &parser) {
        if (parser["-optimize-level"].isParsed()) {
            OPTIMIZE_LEVEL = parser["-optimize-level"];
        }
        if (parser["-permute-qubit"].isParsed()) {
            PERMUTE_QUBITS = parser["-permute-qubit"];
        }
        if (parser["-block-size"].isParsed()) {
            size_t blockSize = parser["-block-size"];
            if (blockSize == 0)
                cerr << "Error: block size value should > 0, skipping this option!!\n";
            else
                BLOCK_SIZE = blockSize;
        }
        if (parser["-filter-cx"].isParsed()) {
            FILTER_DUPLICATED_CXS = parser["-filter-cx"];
        }
        if (parser["-frontier-sorted"].isParsed()) {
            SORT_FRONTIER = parser["-frontier-sorted"];
        }
        if (parser["-neighbors-sorted"].isParsed()) {
            SORT_NEIGHBORS = parser["-neighbors-sorted"];
        }
        return CMD_EXEC_DONE;
    };
    return cmd;
}
