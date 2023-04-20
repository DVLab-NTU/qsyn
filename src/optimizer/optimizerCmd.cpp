/****************************************************************************
  FileName     [ optimizerCmd.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define optimizer package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "optimizerCmd.h"

#include <cstddef>      // for size_t
#include <iostream>     // for ostream
#include <string>       // for string

#include "optimizer.h"  // for Extractor
#include "qcir.h"       // for QCir
#include "qcirCmd.h"    // for QC_CMD_ID_VALID_OR_RETURN, QC_CMD_QCIR_ID_EX...
#include "qcirMgr.h"    // for QCirMgr
#include "util.h"       // for myStr2Uns

using namespace std;
extern size_t verbose;
extern int effLimit;
extern QCirMgr *qcirMgr;

bool initOptimizeCmd() {
    if (!(cmdMgr->regCmd("OPTimize", 3, make_unique<OptimizeCmd>()))) {
        cerr << "Registering \"optimize\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    Optimize
//----------------------------------------------------------------------
CmdExecStatus
OptimizeCmd::exec(const string &option) {
    QC_CMD_MGR_NOT_EMPTY_OR_RETURN("OPTimize");
    Optimizer Opt(qcirMgr->getQCircuit());
    QCir *result = Opt.parseCircuit(true, false, 1000);
    if (result == nullptr) {
        cout << "Error: Optimize circuit fail." << endl;
    } else {
        qcirMgr->setQCircuit(result);
    }
    return CMD_EXEC_DONE;
}

void OptimizeCmd::usage() const {
    cout << "Usage: Optimize (Under construction)" << endl;
}

void OptimizeCmd::summary() const {
    cout << setw(15) << left << "Optimize: "
         << "Optimize QCir" << endl;
}