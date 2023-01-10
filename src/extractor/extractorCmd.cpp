/****************************************************************************
  FileName     [ m2Cmd.cpp ]
  PackageName  [ m2 ]
  Synopsis     [ Define basic m2 package commands ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "extractorCmd.h"

#include <cassert>
#include <iomanip>
#include <iostream>

#include "extract.h"
#include "qcirCmd.h"
#include "qcirMgr.h"
#include "util.h"
#include "zxCmd.h"
#include "zxGraphMgr.h"

using namespace std;
extern size_t verbose;
extern int effLimit;
extern ZXGraphMgr *zxGraphMgr;
extern QCirMgr *qcirMgr;

bool initExtractCmd() {
    if (!(cmdMgr->regCmd("ZX2QC", 5, new ExtractCmd) &&
          cmdMgr->regCmd("EXTRact", 4, new ExtractStepCmd) &&
          cmdMgr->regCmd("EXTPrint", 4, new ExtractPrintCmd))) {
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
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;
    unsigned id = qcirMgr->getNextID();
    if (!token.empty()) {
        if (!myStr2Uns(option, id)) {
            cerr << "Error: invalid QCir ID!!\n";
            return errorOption(CMD_OPT_ILLEGAL, (option));
        }
    }

    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZX2QC");

    if (!zxGraphMgr->getGraph()->isGraphLike()) {
        cerr << "Error: ZX-graph (id: " << zxGraphMgr->getGraph()->getId() << ") is not graph-like. Not extractable!!" << endl;
        return CMD_EXEC_ERROR;
    }
    zxGraphMgr->copy(zxGraphMgr->getNextID());
    Extractor ext(zxGraphMgr->getGraph());
    QCir *result = ext.extract();
    if (result != nullptr) {
        qcirMgr->addQCir(id);
        qcirMgr->setQCircuit(result);
    }
    return CMD_EXEC_DONE;
}

void ExtractCmd::usage(ostream &os) const {
    os << "Usage: ZX2QC" << endl;
}

void ExtractCmd::help() const {
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

void ExtractStepCmd::usage(ostream &os) const {
    os << "Usage: EXTRact <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-Loop> [(size_t #loop)]" << endl;
    os << "       EXTRact <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-CX | -CZ | -CLFrontier | -RMGadget| -PHase | -H | -PERmute>" << endl;
}

void ExtractStepCmd::help() const {
    cout << setw(15) << left << "EXTRact: "
         << "perform step(s) in extraction" << endl;
}

//----------------------------------------------------------------------
//    EXTPrint <-Frontier | -Neighbors | -Axels | -Matrix>
//----------------------------------------------------------------------
CmdExecStatus
ExtractPrintCmd::exec(const string &option) {
    string token;
    if (!CmdExec::lexSingleOption(option, token)) return CMD_EXEC_ERROR;

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
        MATRIX,
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

void ExtractPrintCmd::usage(ostream &os) const {
    os << "Usage: EXTPrint <-Frontier | -Neighbors | -Axels | -Matrix>" << endl;
}

void ExtractPrintCmd::help() const {
    cout << setw(15) << left << "EXTPrint: "
         << "print info of extracting ZX-graph" << endl;
}