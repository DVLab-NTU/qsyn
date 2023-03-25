/****************************************************************************
  FileName     [ cmdCommon.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Define common commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "cmdCommon.h"

#include <stdlib.h>  // for srand

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "apCmd.h"
#include "myUsage.h"
#include "util.h"

using namespace std;
extern size_t verbose;
extern size_t colorLevel;
extern MyUsage myUsage;

using namespace ArgParse;

unique_ptr<ArgParseCmdType> helpCmd();
unique_ptr<ArgParseCmdType> quitCmd();
unique_ptr<ArgParseCmdType> dofileCmd();
unique_ptr<ArgParseCmdType> usageCmd();
unique_ptr<ArgParseCmdType> seedCmd();
unique_ptr<ArgParseCmdType> colorCmd();
unique_ptr<ArgParseCmdType> historyCmd();

bool initCommonCmd() {
    if (!(cmdMgr->regCmd("QQuit", 2, quitCmd()) &&
          cmdMgr->regCmd("HIStory", 3, historyCmd()) &&
          cmdMgr->regCmd("HELp", 3, helpCmd()) &&
          cmdMgr->regCmd("DOfile", 2, dofileCmd()) &&
          cmdMgr->regCmd("USAGE", 5, usageCmd()) &&
          cmdMgr->regCmd("VERbose", 3, make_unique<VerboseCmd>()) &&
          cmdMgr->regCmd("SEED", 4, make_unique<SeedCmd>()) &&
          cmdMgr->regCmd("COLOR", 5, make_unique<ColorCmd>()))) {
        cerr << "Registering \"init\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

unique_ptr<ArgParseCmdType> helpCmd() {
    auto cmd = make_unique<ArgParseCmdType>("HELp");
    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("shows helping message to commands");

        parser.addArgument<string>("command")
            .defaultValue("")
            .required(false)
            .help("if specified, display help message to a command");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        string command = parser["command"];
        if (command.empty()) {
            cmdMgr->printHelps();
        } else {
            CmdExec* e = cmdMgr->getCmd(parser["command"]);
            if (!e) return CmdExec::errorOption(CMD_OPT_ILLEGAL, parser["command"]);
            e->help();
        }
        return CMD_EXEC_DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> quitCmd() {
    auto cmd = make_unique<ArgParseCmdType>("QQuit");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("quit Qsyn");

        parser.addArgument<bool>("-force")
            .action(storeTrue)
            .help("quit without reaffirming");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        bool forced = parser["-force"];
        if (forced) return CMD_EXEC_QUIT;

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
        return CMD_EXEC_DONE;          // not yet to quit
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> historyCmd() {
    auto cmd = make_unique<ArgParseCmdType>("HIStory");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print command history");
        parser.addArgument<unsigned>("nPrint")
            .required(false)
            .help("if specified, print the <nprint> latest command history");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        unsigned nPrint = parser["nPrint"];
        if (parser["nPrint"].isParsed()) {
            cmdMgr->printHistory(nPrint);
        } else {
            cmdMgr->printHistory();
        }
        return CMD_EXEC_DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> dofileCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DOfile");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("execute the commands in the dofile");

        parser.addArgument<string>("file")
            .help("path to a dofile, i.e., a list of Qsyn commands");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        return cmdMgr->openDofile(parser["file"])
                   ? CMD_EXEC_DONE
                   : CmdExec::errorOption(CMD_OPT_FOPEN_FAIL, parser["file"]);
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> usageCmd() {
    auto cmd = make_unique<ArgParseCmdType>("USAGE");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("report the runtime and/or memory usage");

        // auto mutex = parser.addMutuallyExclusiveGroup();

        parser.addArgument<bool>("-all")
            .action(storeTrue)
            .help("print both time and memory usage");
        parser.parse("-a");
        parser.addArgument<bool>("-time")
            .action(storeTrue)
            .help("print time usage");
        parser.parse("-a");
        parser.addArgument<bool>("-memory")
            .action(storeTrue)
            .help("print memory usage");
        parser.parse("-a");

        

    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        bool repAll = parser["-all"];
        // bool repTime = parser["-time"];
        // bool repMem = parser["-memory"];
        bool repTime = false;
        bool repMem = false;

        parser.printArguments();

        if (!repAll && !repTime && !repMem) repAll = true;

        if (repAll) repTime = true, repMem = true;

        myUsage.report(repTime, repMem);

        return CMD_EXEC_DONE;
    };

    return cmd;
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

void UsageCmd::usage() const {
    cout << "Usage: USAGE [-All | -Time | -Memory]" << endl;
}

void UsageCmd::summary() const {
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

void VerboseCmd::usage() const {
    cout << "Usage: VERbose <size_t verbose level>" << endl;
}

void VerboseCmd::summary() const {
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

void SeedCmd::usage() const {
    cout << "Usage: SEED [size_t seed]" << endl;
}

void SeedCmd::summary() const {
    cout << setw(15) << left << "SEED: "
         << "fix the seed" << endl;
}

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

void ColorCmd::usage() const {
    cout << "Usage: COLOR <bool colored>" << endl;
}

void ColorCmd::summary() const {
    cout << setw(15) << left << "COLOR: "
         << "toggle colored printing (1: on, 0: off)" << endl;
}