/****************************************************************************
  FileName     [ tensorCmd.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "tensorCmd.h"

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>

#include "phase.h"
#include "tensorMgr.h"
#include "textFormat.h"

using namespace std;
namespace TF = TextFormat;

extern TensorMgr *tensorMgr;
extern size_t verbose;

bool initTensorCmd() {
    if (!(
            cmdMgr->regCmd("TSReset", 3, new TSResetCmd) &&
            cmdMgr->regCmd("TSPrint", 3, new TSPrintCmd) &&
            cmdMgr->regCmd("TSADJoint", 3, new TSAdjointCmd) &&
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
    if (!tensorMgr)
        tensorMgr = new TensorMgr;
    else
        tensorMgr->reset();

    return CMD_EXEC_DONE;
}

void TSResetCmd::usage() const {
    cout << "Usage: TSReset" << endl;
}

void TSResetCmd::help() const {
    cout << setw(15) << left << "TSReset: "
         << "reset the tensor manager" << endl;
}

//----------------------------------------------------------------------
//    TSPrint [-List] [size_t id]
//----------------------------------------------------------------------
CmdExecStatus
TSPrintCmd::exec(const string &option) {
    vector<string> options;
    if (!lexOptions(option, options)) {
        return CMD_EXEC_ERROR;
    }

    if (!tensorMgr) tensorMgr = new TensorMgr;

    bool list = false;
    bool useId = false;
    unsigned id;
    for (const auto &option : options) {
        if (myStrNCmp("-List", option, 2) == 0) {
            if (list) {
                return errorOption(CMD_OPT_EXTRA, option);
            }
            list = true;
        } else {
            if (useId) {
                return errorOption(CMD_OPT_EXTRA, option);
            }
            if (!myStr2Uns(option, id)) {
                return errorOption(CMD_OPT_ILLEGAL, option);
            }
            useId = true;
        }
    }
    if (!useId) {
        tensorMgr->printTensorMgr();
    } else {
        if (!tensorMgr->hasId(id)) {
            cerr << "[Error] Can't find tensor with the ID specified!!" << endl;
            return errorOption(CMD_OPT_ILLEGAL, to_string(id));
        }
        tensorMgr->printTensor(id, list);
    }
    return CMD_EXEC_DONE;
}

void TSPrintCmd::usage() const {
    cout << "Usage: TSPrint [-List] [size_t id]" << endl
         << "       -List: List infos only" << endl
         << "       id   : Print the tensor with the id specified" << endl
         << "       If no argument is given, list infos of all stored tensors. " << endl;
}

void TSPrintCmd::help() const {
    cout << setw(15) << left << "TSPrint: "
         << "print info of stored tensors" << endl;
}

//----------------------------------------------------------------------
//    TSEQuiv <size_t tensorId1> <size_t tensorId2> [<-Epsilon> <double eps>] [-Strict]
//----------------------------------------------------------------------
CmdExecStatus
TSEquivalenceCmd::exec(const string &option) {
    vector<string> options;
    if (!lexOptions(option, options)) {
        return CMD_EXEC_ERROR;
    }

    if (!tensorMgr) tensorMgr = new TensorMgr;

    bool useEpsilon = false, nextEpsilon = false;
    double epsilon = 1e-6;
    bool useStrict = false;
    vector<unsigned> ids;
    unsigned tmp;
    ids.reserve(2);

    for (const auto &option : options) {
        if (nextEpsilon) {
            if (!myStr2Double(option, epsilon)) {
                return errorOption(CMD_OPT_ILLEGAL, option);
            }
            useEpsilon = true;
            nextEpsilon = false;
        } else if (myStrNCmp("-Epsilon", option, 2) == 0) {
            if (useEpsilon) {
                return errorOption(CMD_OPT_EXTRA, option);
            }
            nextEpsilon = true;
        } else if (myStrNCmp("-Strict", option, 2) == 0) {
            if (useStrict) {
                return errorOption(CMD_OPT_EXTRA, option);
            }
            useStrict = true;
        } else {
            if (ids.size() >= 2) {
                return errorOption(CMD_OPT_EXTRA, option);
            }
            if (!myStr2Uns(option, tmp)) {
                return errorOption(CMD_OPT_ILLEGAL, option);
            }
            if (!tensorMgr->hasId(tmp)) {
                cerr << "[Error] Can't find tensor with the ID specified!!" << endl;
                return errorOption(CMD_OPT_ILLEGAL, to_string(tmp));
            }
            ids.push_back(tmp);
        }
    }
    if (ids.size() < 2) {
        cerr << "Please give two tensor ids to compare!!" << endl;
        return CMD_EXEC_ERROR;
    }
    if (nextEpsilon) {
        return errorOption(CMD_OPT_MISSING, options.back());
    }
    bool equiv = tensorMgr->isEquivalent(ids[0], ids[1], epsilon);
    double norm = tensorMgr->getGlobalNorm(ids[0], ids[1]);
    Phase phase = tensorMgr->getGlobalPhase(ids[0], ids[1]);

    if (useStrict) {
        if (norm > 1 + epsilon || norm < 1 - epsilon || phase != Phase(0)) {
            equiv = false;
        }
    }

    if (equiv) {
        cout << TF::BOLD(TF::GREEN("Equivalent")) << endl
             << "- Global Norm : " << norm << endl
             << "- Global Phase: " << phase << endl;
    } else {
        cout << TF::BOLD(TF::RED("Not Equivalent")) << endl;
    }

    return CMD_EXEC_DONE;
}

void TSEquivalenceCmd::usage() const {
    cout << "Usage: TSEQuiv <size_t id1> <size_t id2> [<-Epsilon> <double eps>] [-Exact]\n"
         << "       -Epsilon: requires cosine similarity between tensors to be higher than (1 - eps) (default to 1e-6)\n"
         << "       -Strict : requires exact equivalence (global scaling factor of 1)" << endl;
}

void TSEquivalenceCmd::help() const {
    cout << setw(15) << left << "TSEQuiv: "
         << "check the equivalency of two stored tensors" << endl;
}

//----------------------------------------------------------------------
//    TSAdjoint <size_t id>
//----------------------------------------------------------------------
CmdExecStatus
TSAdjointCmd::exec(const string &option) {
    string token;
    if (!lexSingleOption(option, token, false)) {
        return errorOption(CMD_OPT_MISSING, "");
    }
    unsigned id;
    if (!myStr2Uns(token, id)) {
        return errorOption(CMD_OPT_ILLEGAL, token);
    }

    if (!tensorMgr) tensorMgr = new TensorMgr;
    if (!tensorMgr->hasId(id)) {
        cerr << "[Error] Can't find tensor with the ID specified!!" << endl;
        return errorOption(CMD_OPT_ILLEGAL, to_string(id));
    }

    tensorMgr->adjoint(id);
    return CMD_EXEC_DONE;
}

void TSAdjointCmd::usage() const {
    cout << "Usage: TSAdjoint <size_t id>" << endl;
}

void TSAdjointCmd::help() const {
    cout << setw(15) << left << "TSADJoint: "
         << "adjoint the specified tensor" << endl;
}
