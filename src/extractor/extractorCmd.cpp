/****************************************************************************
  FileName     [ extractorCmd.cpp ]
  PackageName  [ extractor ]
  Synopsis     [ Define extractor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "extractorCmd.h"

#include <cstddef>        // for size_t
#include <iostream>       // for ostream
#include <string>         // for string

#include "cmdMacros.h"    // for CMD_N_OPTS_EQUAL_OR_RETURN, CMD_N_OPTS_AT_LE...
#include "extract.h"      // for Extractor
#include "qcir.h"         // for QCir
#include "qcirCmd.h"      // for QC_CMD_ID_VALID_OR_RETURN, QC_CMD_QCIR_ID_EX...
#include "qcirMgr.h"      // for QCirMgr
#include "topologyMgr.h"  // for DeviceMgr
#include "util.h"         // for myStr2Uns
#include "zxCmd.h"        // for ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN, ZX_CMD_...
#include "zxGraph.h"      // for ZXGraph
#include "zxGraphMgr.h"   // for ZXGraphMgr

using namespace std;
extern size_t verbose;
extern int effLimit;
extern ZXGraphMgr *zxGraphMgr;
extern QCirMgr *qcirMgr;
extern DeviceMgr *deviceMgr;

bool initExtractCmd() {
    if (!(cmdMgr->regCmd("ZX2QC", 5, make_unique<ExtractCmd>()) &&
          cmdMgr->regCmd("EXTRact", 4, make_unique<ExtractStepCmd>()) &&
          cmdMgr->regCmd("EXTSet", 4, make_unique<ExtractSetCmd>()) &&
          cmdMgr->regCmd("EXTPrint", 4, make_unique<ExtractPrintCmd>()))) {
        cerr << "Registering \"extract\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZX2QC
//----------------------------------------------------------------------
CmdExecStatus
ExtractCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token))
        return CMD_EXEC_ERROR;
    // if (token.empty() || myStrNCmp("-Logical", token, 2) == 0)
    //     topo = Device();
    // else if (myStrNCmp("-Physical", token, 2) == 0) {
    //     if (deviceMgr->getDTListItr() == deviceMgr->getDeviceList().end()) {
    //         cerr << "Error: Device list is empty now. Please DTNEW/DTRead before ZX2QC.\n";
    //         return CMD_EXEC_ERROR;
    //     }
    //     topo = deviceMgr->getDevice();
    // } else {
    //     return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    // }
    unsigned id = qcirMgr->getNextID();
    // if (!token.empty()) {
    //     if (!myStr2Uns(option, id)) {
    //         cerr << "Error: invalid QCir ID!!\n";
    //         return errorOption(CMD_OPT_ILLEGAL, (option));
    //     }
    // }

    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZX2QC");

    if (!zxGraphMgr->getGraph()->isGraphLike()) {
        cerr << "Error: ZX-graph (id: " << zxGraphMgr->getGraph()->getId() << ") is not graph-like. Not extractable!!" << endl;
        return CMD_EXEC_ERROR;
    }
    zxGraphMgr->copy(zxGraphMgr->getNextID());
    Extractor ext(zxGraphMgr->getGraph(), nullptr);
    QCir *result = ext.extract();
    if (result != nullptr) {
        qcirMgr->addQCir(id);
        qcirMgr->setQCircuit(result);
    }
    return CMD_EXEC_DONE;
}

void ExtractCmd::usage() const {
    cout << "Usage: ZX2QC" << endl;
}

void ExtractCmd::summary() const {
    cout << setw(15) << left << "ZX2QC: "
         << "extract QCir from ZX-graph" << endl;
}

//----------------------------------------------------------------------
//    EXTRact <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-Loop> [(size_t #loop)]
//    EXTRact <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-CX | -CZ | -CLFrontier | -RMGadget| -PHase | -H | -PERmute>
//----------------------------------------------------------------------
CmdExecStatus
ExtractStepCmd::exec(const string &option) {
    string token;
    vector<string> options;
    if (!CmdExec::lexOptions(option, options))
        return CMD_EXEC_ERROR;
    if (options.size() == 0) {
        return CmdExec::errorOption(CMD_OPT_MISSING, "");
    }
    if (options.size() < 5) {
        return CmdExec::errorOption(CMD_OPT_MISSING, options[options.size() - 1]);
    }
    if (options.size() > 5) {
        if (myStrNCmp("-Loop", options[4], 2) == 0) {
            if (options.size() > 6) return CmdExec::errorOption(CMD_OPT_EXTRA, options[6]);
        } else
            return CmdExec::errorOption(CMD_OPT_EXTRA, options[5]);
    }
    if (!(myStrNCmp("-ZXgraph", options[0], 3) == 0) || !(myStrNCmp("-QCir", options[2], 3) == 0)) {
        return CMD_EXEC_ERROR;
    }
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("EXtract");
    unsigned id;
    ZX_CMD_ID_VALID_OR_RETURN(options[1], id, "Graph");
    ZX_CMD_GRAPH_ID_EXISTS_OR_RETURN(id);
    zxGraphMgr->checkout2ZXGraph(id);
    if (!zxGraphMgr->getGraph()->isGraphLike()) {
        cerr << "Error: ZX-graph (id: " << zxGraphMgr->getGraph()->getId() << ") is not graph-like. Not extractable!!" << endl;
        return CMD_EXEC_ERROR;
    }

    QC_CMD_ID_VALID_OR_RETURN(options[3], id, "QCir");
    QC_CMD_QCIR_ID_EXISTS_OR_RETURN(id);
    qcirMgr->checkout2QCir(id);

    if (zxGraphMgr->getGraph()->getNumOutputs() != qcirMgr->getQCircuit()->getNQubit()) {
        cerr << "Error: number of outputs in graph is not equal to number of qubits in circuit" << endl;
        return CMD_EXEC_ERROR;
    }

    enum class EXTRACT_MODE {
        LOOP,
        PHASE,
        CZ,
        CLEAN_FRONTIER,
        RM_GADGET,
        GAUSSIAN_CX,
        H,
        PERMUTE_QUBITS,
        ERROR
    };
    EXTRACT_MODE mode;
    unsigned opt;
    if (myStrNCmp("-Loop", options[4], 2) == 0) {
        if (options.size() == 5)
            opt = 1;
        else
            QC_CMD_ID_VALID_OR_RETURN(options[5], opt, "option");
        mode = EXTRACT_MODE::LOOP;
    } else if (myStrNCmp("-CX", options[4], 3) == 0) {
        mode = EXTRACT_MODE::GAUSSIAN_CX;
    } else if (myStrNCmp("-CZ", options[4], 3) == 0) {
        mode = EXTRACT_MODE::CZ;
    } else if (myStrNCmp("-CLFrontier", options[4], 4) == 0) {
        mode = EXTRACT_MODE::CLEAN_FRONTIER;
    } else if (myStrNCmp("-RMGadget", options[4], 4) == 0) {
        mode = EXTRACT_MODE::RM_GADGET;
    } else if (myStrNCmp("-PHase", options[4], 3) == 0) {
        mode = EXTRACT_MODE::PHASE;
    } else if (myStrNCmp("-H", options[4], 2) == 0) {
        mode = EXTRACT_MODE::H;
    } else if (myStrNCmp("-PERmute", options[4], 4) == 0) {
        mode = EXTRACT_MODE::PERMUTE_QUBITS;
    } else {
        cout << "Error: unsupported option " << options[4] << " !!" << endl;
        return CMD_EXEC_ERROR;
    }

    Extractor ext(zxGraphMgr->getGraph(), qcirMgr->getQCircuit());

    switch (mode) {
        case EXTRACT_MODE::LOOP:
            ext.extractionLoop(opt);
            break;
        case EXTRACT_MODE::PHASE:
            ext.extractSingles();
            break;
        case EXTRACT_MODE::CZ:
            ext.extractCZs(true);
            break;
        case EXTRACT_MODE::CLEAN_FRONTIER:
            ext.cleanFrontier();
            break;
        case EXTRACT_MODE::RM_GADGET:
            if (ext.removeGadget(true))
                cout << "Gadget(s) are removed" << endl;
            else
                cout << "No gadget(s) are found" << endl;
            break;
        case EXTRACT_MODE::GAUSSIAN_CX:
            if (ext.gaussianElimination(true)) {
                ext.updateGraphByMatrix();
                ext.extractCXs();
            }
            break;
        case EXTRACT_MODE::H:
            ext.extractHsFromM2(true);
            break;
        case EXTRACT_MODE::PERMUTE_QUBITS:
            ext.permuteQubit();
            break;
        default:
            break;
    }

    return CMD_EXEC_DONE;
}

void ExtractStepCmd::usage() const {
    cout << "Usage: EXTRact <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-Loop> [(size_t #loop)]" << endl;
    cout << "       EXTRact <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-CX | -CZ | -CLFrontier | -RMGadget| -PHase | -H | -PERmute>" << endl;
}

void ExtractStepCmd::summary() const {
    cout << setw(15) << left << "EXTRact: "
         << "perform step(s) in extraction" << endl;
}

//----------------------------------------------------------------------
//    EXTPrint <-Frontier | -Neighbors | -Axels | -Matrix | -Settings>
//----------------------------------------------------------------------
CmdExecStatus
ExtractPrintCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    if (myStrNCmp("-Settings", option, 2) == 0) {
        cout << endl;
        cout << "Sort Frontier:     " << (SORT_FRONTIER == true ? "True" : "False") << endl;
        cout << "Sort Neighbors:    " << (SORT_NEIGHBORS == true ? "True" : "False") << endl;
        cout << "Permute Qubits:    " << (PERMUTE_QUBITS == true ? "True" : "False") << endl;
        cout << "Filter Duplicated: " << (FILTER_DUPLICATED_CXS == true ? "True" : "False") << endl;
        cout << "Block Size:        " << BLOCK_SIZE << endl;
        cout << "Optimize Level:    " << OPTIMIZE_LEVEL << endl;
        return CMD_EXEC_DONE;
    }
    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("EXTPrint");

    if (!zxGraphMgr->getGraph()->isGraphLike()) {
        cerr << "Error: ZX-graph (id: " << zxGraphMgr->getGraph()->getId() << ") is not graph-like. Not extractable!!" << endl;
        return CMD_EXEC_ERROR;
    }
    Extractor ext(zxGraphMgr->getGraph());

    enum class PRINT_MODE {
        FRONTIER,
        NEIGHBORS,
        AXELS,
        MATRIX
    };
    PRINT_MODE mode;

    if (myStrNCmp("-Frontier", option, 2) == 0) {
        mode = PRINT_MODE::FRONTIER;
    } else if (myStrNCmp("-Neighbors", option, 2) == 0) {
        mode = PRINT_MODE::NEIGHBORS;
    } else if (myStrNCmp("-Axels", option, 1) == 0) {
        mode = PRINT_MODE::AXELS;
    } else if (myStrNCmp("-Matrix", option, 2) == 0) {
        mode = PRINT_MODE::MATRIX;
    } else {
        cout << "Error: unsupported option " << option << " !!" << endl;
        return CMD_EXEC_ERROR;
    }

    switch (mode) {
        case PRINT_MODE::FRONTIER:
            ext.printFrontier();
            break;
        case PRINT_MODE::NEIGHBORS:
            ext.printNeighbors();
            break;
        case PRINT_MODE::AXELS:
            ext.printAxels();
            break;
        case PRINT_MODE::MATRIX:
            ext.createMatrix();
            ext.printMatrix();
            break;
        default:
            break;
    }
    return CMD_EXEC_DONE;
}

void ExtractPrintCmd::usage() const {
    cout << "Usage: EXTPrint <-Frontier | -Neighbors | -Axels | -Matrix>" << endl;
}

void ExtractPrintCmd::summary() const {
    cout << setw(15) << left << "EXTPrint: "
         << "print info of extracting ZX-graph" << endl;
}

//------------------------------------------------------------------------------
//    EXTSet <bool sortFrontier> <bool sortNeighbors> <bool permuteQubits> <size_t block size> <bool filtered> <size_t optimizeLevel>
//------------------------------------------------------------------------------
CmdExecStatus
ExtractSetCmd::exec(const string &option) {
    string token;
    vector<string> options;
    if (!CmdExec::lexOptions(option, options))
        return CMD_EXEC_ERROR;
    CMD_N_OPTS_EQUAL_OR_RETURN(options, 6);
    unsigned sortFrontier, sortNeighbors, permuteQubits, blockSize, filterDuplicated, optimizeLevel;
    if (!myStr2Uns(options[0], sortFrontier)) {
        cerr << "Error: invalid sortFrontier value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (sortFrontier != 0 && sortFrontier != 1) {
        cerr << "Error: invalid sortFrontier value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (!myStr2Uns(options[1], sortNeighbors)) {
        cerr << "Error: invalid sortNeighbors value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (sortNeighbors != 0 && sortNeighbors != 1) {
        cerr << "Error: invalid sortNeighbors value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (!myStr2Uns(options[2], permuteQubits)) {
        cerr << "Error: invalid permuteQubits value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (permuteQubits != 0 && permuteQubits != 1) {
        cerr << "Error: invalid permuteQubits value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (!myStr2Uns(options[3], blockSize)) {
        cerr << "Error: invalid blockSize value!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (!myStr2Uns(options[4], filterDuplicated)) {
        cerr << "Error: invalid filterDuplicated value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (filterDuplicated != 0 && filterDuplicated != 1) {
        cerr << "Error: invalid filterDuplicated value, should be 0 or 1!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    if (!myStr2Uns(options[5], optimizeLevel)) {
        cerr << "Error: invalid optimizeLevel value!!\n";
        return errorOption(CMD_OPT_ILLEGAL, (option));
    }
    SORT_FRONTIER = sortFrontier == 1;
    SORT_NEIGHBORS = sortNeighbors == 1;
    PERMUTE_QUBITS = permuteQubits == 1;
    FILTER_DUPLICATED_CXS = filterDuplicated == 1;
    BLOCK_SIZE = blockSize;
    OPTIMIZE_LEVEL = optimizeLevel;
    return CMD_EXEC_DONE;
}

void ExtractSetCmd::usage() const {
    cout << "Usage: EXTSet <bool sortFrontier> <bool sortNeighbors> <bool permuteQubits> <size_t block size> <bool filtered> <size_t optimizeLevel>" << endl;
}

void ExtractSetCmd::summary() const {
    cout << setw(15) << left << "EXTSet: "
         << "Set variables to extractor" << endl;
}