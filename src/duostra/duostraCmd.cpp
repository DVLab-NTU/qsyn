/****************************************************************************
  FileName     [ duostraCmd.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "duostraCmd.h"

#include <cstddef>        // for size_t
#include <iostream>       // for ostream
#include <string>         // for string

#include "cmdMacros.h"    // for CMD_N_OPTS_EQUAL_OR_RETURN, CMD_N_OPTS_AT_LE...
#include "duostra.h"      // for Duostra
#include "qcir.h"         // for QCir
#include "qcirCmd.h"      // for QC_CMD_ID_VALID_OR_RETURN, QC_CMD_QCIR_ID_EX...
#include "qcirMgr.h"      // for QCirMgr
#include "topologyMgr.h"  // for DeviceMgr
#include "util.h"         // for myStr2Uns

using namespace std;
extern size_t verbose;
extern int effLimit;
extern QCirMgr *qcirMgr;
extern DeviceMgr *deviceMgr;

bool initDuostraCmd() {
    if (!(cmdMgr->regCmd("DUOSTRA", 7, make_unique<DuostraCmd>()))) {
        cerr << "Registering \"Duostra\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
//    DUOSTRA
//------------------------------------------------------------------------------
CmdExecStatus
DuostraCmd::exec(const string &option) {
    if (!CmdExec::lexNoOption(option))
        return CMD_EXEC_ERROR;

    QC_CMD_MGR_NOT_EMPTY_OR_RETURN("DUOSTRA");
    Duostra duo = Duostra(qcirMgr->getQCircuit(), deviceMgr->getDevice());
    duo.flow();
    return CMD_EXEC_DONE;
}

void DuostraCmd::usage() const {
    cout << "Usage: DUOSTRA" << endl;
}

void DuostraCmd::summary() const {
    cout << setw(15) << left << "DUOSTRA: "
         << "Map logical circuit to physical circuit" << endl;
}