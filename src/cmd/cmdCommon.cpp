/****************************************************************************
  FileName     [ cmdCommon.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Define common commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "cmdCommon.h"

#include <stdlib.h>  // for srand

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>

#include "myUsage.h"
#include "util.h"

using namespace std;
extern size_t verbose;
extern size_t colorLevel;

bool initCommonCmd() {
    if (!(cmdMgr->regCmd("QQuit", 2, new QuitCmd) &&
          cmdMgr->regCmd("HIStory", 3, new HistoryCmd) &&
          cmdMgr->regCmd("HELp", 3, new HelpCmd) &&
          cmdMgr->regCmd("DOfile", 2, new DofileCmd) &&
          cmdMgr->regCmd("USAGE", 5, new UsageCmd) &&
          cmdMgr->regCmd("VERbose", 3, new VerboseCmd) &&
          cmdMgr->regCmd("SEED", 4, new SeedCmd) &&
          cmdMgr->regCmd("COLOR", 5, new ColorCmd))) {
        cerr << "Registering \"init\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    HELp [(string cmd)]
//----------------------------------------------------------------------
CmdExecStatus
HelpCmd::exec(const string& option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token))
        return CMD_EXEC_ERROR;
    if (token.size()) {
        CmdExec* e = cmdMgr->getCmd(token);
        if (!e) return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        e->help();
        e->usage(cout);
    } else
        cmdMgr->printHelps();
    return CMD_EXEC_DONE;
}

void HelpCmd::usage(ostream& os) const {
    os << "Usage: HELp [(string cmd)]" << endl;
}

void HelpCmd::help() const {
    cout << setw(15) << left << "HELp: "
         << "print this help message" << endl;
}

//----------------------------------------------------------------------
//    QQuit [-Force]
//----------------------------------------------------------------------
CmdExecStatus
QuitCmd::exec(const string& option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token))
        return CMD_EXEC_ERROR;
    if (token.size()) {
        if (myStrNCmp("-Forced", token, 2) != 0)
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        else
            return CMD_EXEC_QUIT;  // ready to quit
    }

    cout << "Are you sure to quit (Yes/No)? [No] ";
    char str[1024];
    cin.getline(str, 1024);
    string ss = string(str);
    size_t s = ss.find_first_not_of(' ', 0);
    if (s != string::npos) {
        ss = ss.substr(s);
        if (myStrNCmp("Yes", ss, 1) == 0)
            return CMD_EXEC_QUIT;  // ready to quit
    }
    return CMD_EXEC_DONE;  // not yet to quit
}

void QuitCmd::usage(ostream& os) const {
    os << "Usage: QQuit [-Force]" << endl;
}

void QuitCmd::help() const {
    cout << setw(15) << left << "QQuit: "
         << "quit Qsyn" << endl;
}

//----------------------------------------------------------------------
//    HIStory [(int nPrint)]
//----------------------------------------------------------------------
CmdExecStatus
HistoryCmd::exec(const string& option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token))
        return CMD_EXEC_ERROR;
    int nPrint = -1;
    if (token.size()) {
        if (!myStr2Int(token, nPrint))
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }

    cmdMgr->printHistory(nPrint);

    return CMD_EXEC_DONE;
}

void HistoryCmd::usage(ostream& os) const {
    os << "Usage: HIStory [(int nPrint)]" << endl;
}

void HistoryCmd::help() const {
    cout << setw(15) << left << "HIStory: "
         << "print command history" << endl;
}

//----------------------------------------------------------------------
//    DOfile <(string file)>
//----------------------------------------------------------------------

CmdExecStatus
DofileCmd::exec(const string& option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token, false))
        return CMD_EXEC_ERROR;
    if (!cmdMgr->openDofile(token))
        return CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, token);
    return CMD_EXEC_DONE;
}

void DofileCmd::usage(ostream& os) const {
    os << "Usage: DOfile <(string file)>" << endl;
}

void DofileCmd::help() const {
    cout << setw(15) << left << "DOfile: "
         << "execute the commands in the dofile" << endl;
}

//----------------------------------------------------------------------
//    USAGE [-All | -Time | -Memory]
//----------------------------------------------------------------------
CmdExecStatus
UsageCmd::exec(const string& option) {
    // check option
    vector<string> options;
    CmdExec::lexOptions(option, options);

    bool repTime = false, repMem = false, repAll = false;
    size_t n = options.size();
    if (n == 0)
        repAll = true;
    else {
        for (size_t i = 0; i < n; ++i) {
            const string& token = options[i];
            if (myStrNCmp("-All", token, 2) == 0) {
                if (repTime | repMem | repAll)
                    return CmdExec::errorOption(CMD_OPT_EXTRA, token);
                repAll = true;
            } else if (myStrNCmp("-Time", token, 2) == 0) {
                if (repTime | repMem | repAll)
                    return CmdExec::errorOption(CMD_OPT_EXTRA, token);
                repTime = true;
            } else if (myStrNCmp("-Memory", token, 2) == 0) {
                if (repTime | repMem | repAll)
                    return CmdExec::errorOption(CMD_OPT_EXTRA, token);
                repMem = true;
            } else
                return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
    }

    if (repAll) repTime = repMem = true;

    myUsage.report(repTime, repMem);

    return CMD_EXEC_DONE;
}

void UsageCmd::usage(ostream& os) const {
    os << "Usage: USAGE [-All | -Time | -Memory]" << endl;
}

void UsageCmd::help() const {
    cout << setw(15) << left << "USAGE: "
         << "report the runtime and/or memory usage" << endl;
}

//----------------------------------------------------------------------
//    VERbose <size_t verbose level>
//----------------------------------------------------------------------
CmdExecStatus
VerboseCmd::exec(const string& option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token, false))
        return CMD_EXEC_ERROR;
    unsigned level;
    if (!myStr2Uns(token, level)) {
        cerr << "Error: verbose level should be a positive integer or 0!!" << endl;
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    if (level > 9 && level != 353) {
        cerr << "Error: verbose level should be 0-9 !!" << endl;
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    cout << "Note: verbose level is set to " << level << endl;
    verbose = level;
    return CMD_EXEC_DONE;
}

void VerboseCmd::usage(ostream& os) const {
    os << "Usage: VERbose <size_t verbose level>" << endl;
}

void VerboseCmd::help() const {
    cout << setw(15) << left << "VERbose: "
         << "set verbose level to 0-9 (default: 3)" << endl;
}

//----------------------------------------------------------------------
//    SEED [size_t seed]
//----------------------------------------------------------------------
CmdExecStatus
SeedCmd::exec(const string& option) {
    // check option
    string token;
    if (option.size() == 0) {
        srand(353);
        cerr << "Note: seed is set to 353" << endl;
    } else {
        if (!CmdExec::lexSingleOption(option, token, false))
            return CMD_EXEC_ERROR;
        int seed;
        if (!myStr2Int(token, seed)) {
            cerr << "Error: Seed should be an integer!!" << endl;
            return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
        }
        srand(seed);
        cerr << "Note: seed is set to " << seed << endl;
    }
    return CMD_EXEC_DONE;
}

void SeedCmd::usage(ostream& os) const {
    os << "Usage: SEED [size_t seed]" << endl;
}

void SeedCmd::help() const {
    cout << setw(15) << left << "SEED: "
         << "fix the seed" << endl;
}

//----------------------------------------------------------------------

//----------------------------------------------------------------------
//    COLOR <size_t color level>
//----------------------------------------------------------------------
CmdExecStatus
ColorCmd::exec(const string& option) {
    // check option
    string token;
    if (!CmdExec::lexSingleOption(option, token, false))
        return CMD_EXEC_ERROR;
    unsigned level;
    if (!myStr2Uns(token, level)) {
        cerr << "Error: colored should be a positive integer or 0!!" << endl;
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    if (level > 1) {
        cerr << "Error: colored should be 0-1 !!" << endl;
        return CmdExec::errorOption(CMD_OPT_ILLEGAL, token);
    }
    cout << "Note: colored is set to " << level << endl;
    colorLevel = level;
    return CMD_EXEC_DONE;
}

void ColorCmd::usage(ostream& os) const {
    os << "Usage: COLOR <bool colored>" << endl;
}

void ColorCmd::help() const {
    cout << setw(15) << left << "COLOR: "
         << "toggle colored printing (1: on, 0: off)" << endl;
}
