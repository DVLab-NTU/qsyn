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
#include "qcirMgr.h"
#include "util.h"
#include "zxGraphMgr.h"
#include "qcirCmd.h"
#include "zxCmd.h"

using namespace std;
extern size_t verbose;
extern int effLimit;
extern ZXGraphMgr *zxGraphMgr;
extern QCirMgr *qcirMgr;

bool initExtractCmd() {
    if (!(cmdMgr->regCmd("ZX2QC", 5, new ExtractCmd) &&
          cmdMgr->regCmd("EXTStep", 4, new ExtractStepCmd))) {
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

    if(!zxGraphMgr->getGraph()->isGraphLike()){
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
         << "extract the circuit from the ZX-graph" << endl;
}

//----------------------------------------------------------------------
//    EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-Loop>             [(size_t #loop)]
//    EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-CX>               [(size_t CX strategies)]
//    EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-CZ | -CLFrontier> [(size_t CZ strategies)]
//    EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-RMGadget| -PHase | -H | -PERmute>
//----------------------------------------------------------------------
CmdExecStatus
ExtractStepCmd::exec(const string &option) {
    string token;
    vector<string> options;
    if (!CmdExec::lexOptions(option, options))
        return CMD_EXEC_ERROR;
    if(options.size() < 5){
        return CmdExec::errorOption(CMD_OPT_MISSING, options[options.size()-1]);
    }
    if(options.size() > 5){
        if (myStrNCmp("-Loop", options[4], 2) == 0){
            if(options.size() > 6) return CmdExec::errorOption(CMD_OPT_EXTRA, options[6]);
        }
        else 
        return CmdExec::errorOption(CMD_OPT_EXTRA, options[5]);
    }
    if ((myStrNCmp("-ZXgraph", options[0], 3) == 0) && (myStrNCmp("-QCir", options[2], 3) == 0)) {
        return CMD_EXEC_ERROR;
    }

    ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("EXtract");
    unsigned id;
    ZX_CMD_ID_VALID_OR_RETURN(options[1], id, "Graph");
    ZX_CMD_GRAPH_ID_EXISTS_OR_RETURN(id);
    zxGraphMgr->checkout2ZXGraph(id);
    if(!zxGraphMgr->getGraph()->isGraphLike()){
        cerr << "Error: ZX-graph (id: " << zxGraphMgr->getGraph()->getId() << ") is not graph-like. Not extractable!!" << endl;
        return CMD_EXEC_ERROR;
    }

    QC_CMD_ID_VALID_OR_RETURN(options[3], id, "QCir");
    QC_CMD_QCIR_ID_EXISTS_OR_RETURN(id);
    qcirMgr->checkout2QCir(id);
    
    if(zxGraphMgr->getGraph()->getNumOutputs() != qcirMgr->getQCircuit()->getNQubit()){
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
        if(options.size()==5)
            opt = 1;
        else
            QC_CMD_ID_VALID_OR_RETURN(options[5], opt, "option");
        mode = EXTRACT_MODE::LOOP;
    } else if (myStrNCmp("-CX", options[4], 3) == 0) {
        if(options.size()==5)
            opt = 0;
        else
            QC_CMD_ID_VALID_OR_RETURN(options[5], opt, "option");
        mode = EXTRACT_MODE::GAUSSIAN_CX;
    } else if (myStrNCmp("-CZ", options[4], 3) == 0) {
        if(options.size()==5)
            opt = 0;
        else
            QC_CMD_ID_VALID_OR_RETURN(options[5], opt, "option");
        mode = EXTRACT_MODE::CZ;
    } else if (myStrNCmp("-CLFrontier", options[4], 4) == 0) {
        if(options.size()==5)
            opt = 0;
        else
            QC_CMD_ID_VALID_OR_RETURN(options[5], opt, "option");
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
        cout << "Error: unsupported option " << token << " !!" << endl;
        return CMD_EXEC_ERROR;
    }

    Extractor ext(zxGraphMgr->getGraph(), qcirMgr->getQCircuit());

    switch (mode) {
        case EXTRACT_MODE::LOOP:
        default:
            break;
    }

    return CMD_EXEC_DONE;
}

void ExtractStepCmd::usage(ostream &os) const {
    os << "Usage: EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-Loop>             [(size_t #loop)]" << endl;
    os << "       EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-CX>               [(size_t CX strategies)]" << endl;
    os << "       EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-CZ | -CLFrontier> [(size_t CZ strategies)]" << endl;
    os << "       EXtract <-ZXgraph> <(size_t ZX-graphId)> <-QCir> <(size_t QCirId)> <-RMGadget| -PHase | -H | -PERmute>" << endl;
}

void ExtractStepCmd::help() const {
    cout << setw(15) << left << "EXtract: "
         << "perform step(s) in extraction" << endl;
}