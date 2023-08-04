/****************************************************************************
  FileName     [ cliCmd.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define common commands for any CLI ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include "cli.h"
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
unique_ptr<ArgParseCmdType> verboseCmd();
unique_ptr<ArgParseCmdType> seedCmd();
unique_ptr<ArgParseCmdType> colorCmd();
unique_ptr<ArgParseCmdType> historyCmd();
unique_ptr<ArgParseCmdType> clearCmd();

bool initCommonCmd() {
    if (!(cli.regCmd("QQuit", 2, quitCmd()) &&
          cli.regCmd("HIStory", 3, historyCmd()) &&
          cli.regCmd("HELp", 3, helpCmd()) &&
          cli.regCmd("DOfile", 2, dofileCmd()) &&
          cli.regCmd("USAGE", 5, usageCmd()) &&
          cli.regCmd("VERbose", 3, verboseCmd()) &&
          cli.regCmd("SEED", 4, seedCmd()) &&
          cli.regCmd("CLEAR", 5, clearCmd()) &&
          cli.regCmd("COLOR", 5, colorCmd()))) {
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
            .nargs(NArgsOption::OPTIONAL)
            .help("if specified, display help message to a command");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        string command = parser["command"];
        if (command.empty()) {
            cli.printHelps();
        } else {
            CmdExec* e = cli.getCmd(parser["command"]);
            if (!e) {
                cerr << "Error: Illegal command!! (" << parser["command"] << ")\n";
                return CmdExecResult::ERROR;
            }
            e->help();
        }
        return CmdExecResult::DONE;
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
        if (forced) return CmdExecResult::QUIT;

        cout << "Are you sure to quit (Yes/No)? [No] ";
        char str[1024];
        cin.getline(str, 1024);
        string ss = string(str);
        size_t s = ss.find_first_not_of(' ', 0);
        if (s != string::npos) {
            ss = ss.substr(s);
            if (myStrNCmp("Yes", ss, 1) == 0)
                return CmdExecResult::QUIT;  // ready to quit
        }
        return CmdExecResult::DONE;  // not yet to quit
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> historyCmd() {
    auto cmd = make_unique<ArgParseCmdType>("HIStory");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print command history");
        parser.addArgument<size_t>("nPrint")
            .nargs(NArgsOption::OPTIONAL)
            .help("if specified, print the <nprint> latest command history");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["nPrint"].isParsed()) {
            cli.printHistory(parser["nPrint"]);
        } else {
            cli.printHistory();
        }
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> dofileCmd() {
    auto cmd = make_unique<ArgParseCmdType>("DOfile");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("execute the commands in the dofile");

        parser.addArgument<string>("file")
            .constraint(file_exists)
            .help("path to a dofile, i.e., a list of Qsyn commands");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (!cli.openDofile(parser["file"])) {
            cerr << "Error: cannot open file \"" << parser["file"] << "\"!!" << endl;
            return CmdExecResult::ERROR;
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> usageCmd() {
    auto cmd = make_unique<ArgParseCmdType>("USAGE");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("report the runtime and/or memory usage");

        auto mutex = parser.addMutuallyExclusiveGroup();

        mutex.addArgument<bool>("-all")
            .action(storeTrue)
            .help("print both time and memory usage");
        mutex.addArgument<bool>("-time")
            .action(storeTrue)
            .help("print time usage");
        mutex.addArgument<bool>("-memory")
            .action(storeTrue)
            .help("print memory usage");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        bool repAll = parser["-all"];
        bool repTime = parser["-time"];
        bool repMem = parser["-memory"];

        if (!repAll && !repTime && !repMem) repAll = true;

        if (repAll) repTime = true, repMem = true;

        myUsage.report(repTime, repMem);

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> verboseCmd() {
    auto cmd = make_unique<ArgParseCmdType>("VERbose");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("set verbose level to 0-9 (default: 3)");

        parser.addArgument<size_t>("level")
            .constraint({[](size_t const& val) {
                             return val <= 9 || val == 353;
                         },
                         [](size_t const& val) {
                             cerr << "Error: verbose level should be 0-9!!\n";
                         }})
            .help("0: silent, 1-3: normal usage, 4-6: detailed info, 7-9: prolix debug info");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        verbose = parser["level"];
        cout << "Note: verbose level is set to " << parser["level"] << endl;
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> seedCmd() {
    auto cmd = make_unique<ArgParseCmdType>("SEED");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("set the random seed");

        parser.addArgument<unsigned>("seed")
            .defaultValue(353)
            .nargs(NArgsOption::OPTIONAL)
            .help("random seed value");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        srand(parser["seed"]);
        cout << "Note: seed is set to " << parser["seed"] << endl;
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> colorCmd() {
    auto cmd = make_unique<ArgParseCmdType>("COLOR");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("toggle colored printing");

        parser.addArgument<string>("mode")
            .choices({"on", "off"})
            .help("on: colored printing, off: pure-ascii printing");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        string mode = parser["mode"];
        colorLevel = (mode == "on") ? 1 : 0;
        cout << "Note: color mode is set to " << mode << endl;

        return CmdExecResult::DONE;
    };

    return cmd;
};

unique_ptr<ArgParseCmdType> clearCmd() {
    auto cmd = make_unique<ArgParseCmdType>("CLEAR");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("clear the console");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        cli.clearConsole();

        return CmdExecResult::DONE;
    };

    return cmd;
};
