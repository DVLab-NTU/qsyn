/****************************************************************************
  FileName     [ tensorCmd.h ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor commands ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "tensorCmd.h"

#include <vector>

#include "qtensor.h"

using namespace std;

extern TensorMgr tensorMgr;

bool initTensorCmd() {
    if (!(
            cmdMgr->regCmd("TSReset", 3, new TSResetCmd) &&
            cmdMgr->regCmd("TSPrint", 3, new TSPrintCmd) &&
            cmdMgr->regCmd("TSEQuiv", 4, new TSEquivalenceCmd))) {
        cerr << "Registering \"tensor\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    TSReset 
//----------------------------------------------------------------------
CmdExecStatus
TSResetCmd::exec(const string &option) {
    if (!lexNoOption(option)) {
        return CMD_EXEC_ERROR;
    }

    return CMD_EXEC_DONE;
}

void TSResetCmd::usage(ostream &os) const {
    os << "Usage: TSPrint [-List | size_t id]"
       << "       If id is not given, print the summary on all stored tensors" << endl;
}

void TSResetCmd::help() const {
    cout << setw(15) << left << "TSPrint: "
         << "Print information about stored tensors" << endl;
}

//----------------------------------------------------------------------
//    TSPrint [-List | size_t id]
//----------------------------------------------------------------------
CmdExecStatus
TSPrintCmd::exec(const string &option) {
    string token;
    if (!lexSingleOption(option, token, true)) {
        return CMD_EXEC_ERROR;
    }
    if (token.empty() || myStrNCmp("-List", token, 2) == 0) {

    }
    return CMD_EXEC_DONE;
}

void TSPrintCmd::usage(ostream &os) const {
    os << "Usage: TSPrint [-List | size_t id]"
       << "       If id is not given, print the summary on all stored tensors" << endl;
}

void TSPrintCmd::help() const {
    cout << setw(15) << left << "TSPrint: "
         << "Print information about stored tensors" << endl;
}

//----------------------------------------------------------------------
//    TSEQuiv <size_t tensorId1> <size_t tensorId2> [double eps]
//----------------------------------------------------------------------
CmdExecStatus
TSEquivalenceCmd::exec(const string &option) {
    return CMD_EXEC_DONE;
}

void TSEquivalenceCmd::usage(ostream &os) const {
    os << "Usage: TSEQuiv <size_t tensorId1> <size_t tensorId2> [double eps]"
       << "       eps: requires cosine similarity between tensors to be higher than (1 - eps) (default to 1e-6)" << endl;
}

void TSEquivalenceCmd::help() const {
    cout << setw(15) << left << "TSEQuiv: "
         << "Compare the equivalency of two stored tensors" << endl;
}
