/****************************************************************************
  FileName     [ cliCmd.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define common commands for any CLI ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <cstddef>
#include <cstdlib>
#include <string>

#include "cli/cli.hpp"
#include "util/usage.hpp"
#include "util/util.hpp"

using namespace std;
extern size_t verbose;
extern dvlab::utils::Usage usage;

using namespace ArgParse;

unique_ptr<Command> helpCmd();
unique_ptr<Command> quitCmd();
unique_ptr<Command> dofileCmd();
unique_ptr<Command> usageCmd();
unique_ptr<Command> verboseCmd();
unique_ptr<Command> seedCmd();
unique_ptr<Command> historyCmd();
unique_ptr<Command> clearCmd();
unique_ptr<Command> loggerCmd();

bool initCommonCmd() {
    if (!(cli.registerCommand("QQuit", 2, quitCmd()) &&
          cli.registerCommand("HIStory", 3, historyCmd()) &&
          cli.registerCommand("HELp", 3, helpCmd()) &&
          cli.registerCommand("DOfile", 2, dofileCmd()) &&
          cli.registerCommand("USAGE", 5, usageCmd()) &&
          cli.registerCommand("VERbose", 3, verboseCmd()) &&
          cli.registerCommand("SEED", 4, seedCmd()) &&
          cli.registerCommand("CLEAR", 5, clearCmd()) &&
          cli.registerCommand("LOGger", 3, loggerCmd()))) {
        logger.fatal("Registering \"cli\" commands fails... exiting");
        return false;
    }
    return true;
}

unique_ptr<Command> helpCmd() {
    auto cmd = make_unique<Command>("HELp");
    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("shows helping message to commands");

        parser.addArgument<string>("command")
            .defaultValue("")
            .nargs(NArgsOption::OPTIONAL)
            .help("if specified, display help message to a command");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        auto command = parser.get<string>("command");
        if (command.empty()) {
            cli.listAllCommands();
        } else {
            Command* e = cli.getCommand(command);
            if (!e) {
                fmt::println(stderr, "Error: illegal command!! ({})", command);
                return CmdExecResult::ERROR;
            }
            e->help();
        }
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> quitCmd() {
    auto cmd = make_unique<Command>("QQuit");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("quit Qsyn");

        parser.addArgument<bool>("-force")
            .action(storeTrue)
            .help("quit without reaffirming");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.get<bool>("-force")) return CmdExecResult::QUIT;

        std::string const prompt = "Are you sure to quit (Yes/[No])? ";

        if (cli.listenToInput(std::cin, prompt, {.allowBrowseHistory = false, .allowTabCompletion = false}) == CmdExecResult::QUIT) {
            fmt::print("EOF [assumed Yes]");
            return CmdExecResult::QUIT;
        }

        auto input = toLowerString(stripLeadingWhitespaces(cli.getReadBuf()));

        if (input.empty()) return CmdExecResult::DONE;

        return ("yes"s.starts_with(input))
                   ? CmdExecResult::QUIT
                   : CmdExecResult::DONE;  // not yet to quit
    };

    return cmd;
}

unique_ptr<Command> historyCmd() {
    auto cmd = make_unique<Command>("HIStory");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print command history");
        parser.addArgument<size_t>("nPrint")
            .nargs(NArgsOption::OPTIONAL)
            .help("if specified, print the <nprint> latest command history");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser.parsed("nPrint")) {
            cli.printHistory(parser.get<size_t>("nPrint"));
        } else {
            cli.printHistory();
        }
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> dofileCmd() {
    auto cmd = make_unique<Command>("DOfile");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("execute the commands in the dofile");

        parser.addArgument<string>("file")
            .constraint(path_readable)
            .help("path to a dofile, i.e., a list of Qsyn commands");

        parser.addArgument<string>("arguments")
            .nargs(NArgsOption::ZERO_OR_MORE)
            .help("arguments to the dofile");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        auto arguments = parser.get<std::vector<string>>("arguments");
        if (!cli.saveVariables(parser.get<string>("file"), arguments)) {
            return CmdExecResult::ERROR;
        }

        if (!cli.openDofile(parser.get<string>("file"))) {
            logger.error("cannot open file \"{}\"!!", parser.get<std::string>("file"));
            return CmdExecResult::ERROR;
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> usageCmd() {
    auto cmd = make_unique<Command>("USAGE");

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
        auto repAll = parser.get<bool>("-all");
        auto repTime = parser.get<bool>("-time");
        auto repMem = parser.get<bool>("-memory");

        if (!repAll && !repTime && !repMem) repAll = true;

        if (repAll) repTime = true, repMem = true;

        usage.report(repTime, repMem);

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> verboseCmd() {
    auto cmd = make_unique<Command>("VERbose");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("set verbose level to 0-9 (default: 3)");

        parser.addArgument<size_t>("level")
            .constraint({[](size_t const& val) {
                             return val <= 9 || val == 353;
                         },
                         [](size_t const& val) {
                             fmt::println(stderr, "Error: verbose level should be 0-9!!");
                         }})
            .help("0: silent, 1-3: normal usage, 4-6: detailed info, 7-9: prolix debug info");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        verbose = parser.get<size_t>("level");
        fmt::println("Note: verbose level is set to {}", verbose);

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> loggerCmd() {
    auto cmd = make_unique<Command>("LOGger");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        vector<string> logLevels = {"none", "fatal", "error", "warning", "info", "debug", "trace"};
        parser.help("display and set the logger's status");

        auto parsers = parser.addSubParsers()
                           .help("subcommands for logger");

        auto testParser = parsers.addParser("test");
        testParser.help("Test out logger setting");

        auto levelParser = parsers.addParser("level");
        levelParser.help("set logger level");
        levelParser.addArgument<string>("level")
            .constraint(choices_allow_prefix(logLevels))
            .help("set log levels. Levels (ascending): None, Fatal, Error, Warning, Info, Debug, Trace");

        auto historyParser = parsers.addParser("history");
        historyParser.help("print logger history");
        historyParser.addArgument<size_t>("num_history")
            .nargs(NArgsOption::OPTIONAL)
            .metavar("N")
            .help("print log history. If specified, print the lastest N logs");

        auto maskParser = parsers.addParser("mask");
        maskParser.help("set logger mask");
        maskParser.setOptionPrefix("+-");
        auto addFilterGroup = [&maskParser](std::string const& groupName) {
            auto group = maskParser.addMutuallyExclusiveGroup();
            group.addArgument<bool>("+" + groupName)
                .action(storeTrue)
                .help("unmask " + groupName + " logs");
            group.addArgument<bool>("-" + groupName)
                .action(storeTrue)
                .help("mask " + groupName + " logs");
        };

        for (auto& group : logLevels) {
            if (group == "none") continue;
            addFilterGroup(group);
        }
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        using dvlab::utils::Logger;

        if (parser.usedSubParser("test")) {
            logger.fatal("Test fatal log");
            logger.error("Test error log");
            logger.warning("Test warning log");
            logger.info("Test info log");
            logger.debug("Test debug log");
            logger.trace("Test trace log");
            return CmdExecResult::DONE;
        }

        if (parser.usedSubParser("level")) {
            auto level = Logger::str2LogLevel(parser.get<string>("level"));
            assert(level.has_value());
            logger.setLogLevel(*level);
            logger.debug("Setting logger level to {}", Logger::logLevel2Str(*level));
            return CmdExecResult::DONE;
        }

        if (parser.usedSubParser("mask")) {
            vector<string> logLevels = {"fatal", "error", "warning", "info", "debug", "trace"};

            for (auto& group : logLevels) {
                auto level = Logger::str2LogLevel(group);
                assert(level.has_value());
                if (parser.parsed("+" + group)) {
                    logger.unmask(*level);
                    logger.debug("Unmasked logger level: {}", Logger::logLevel2Str(*level));
                } else if (parser.parsed("-" + group)) {
                    logger.mask(*level);
                    logger.debug("Masked logger level: {}", Logger::logLevel2Str(*level));
                }
            }
            return CmdExecResult::DONE;
        }

        if (parser.usedSubParser("history")) {
            if (parser.parsed("num_history")) {
                logger.printLogs(parser.get<size_t>("num_history"));
            } else {
                logger.printLogs();
            }
            return CmdExecResult::DONE;
        }

        fmt::println("Logger Level: {}", Logger::logLevel2Str(logger.getLogLevel()));
        vector<string> masked;
        vector<string> logLevels = {"fatal", "error", "warning", "info", "debug", "trace"};
        for (auto& level : logLevels) {
            if (logger.isMasked(*Logger::str2LogLevel(level))) {
                masked.push_back(level);
            }
        }

        if (masked.size()) {
            fmt::println("Masked logging levels: {}", fmt::join(masked, ", "));
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> seedCmd() {
    auto cmd = make_unique<Command>("SEED");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("set the random seed");

        parser.addArgument<unsigned>("seed")
            .defaultValue(353)
            .nargs(NArgsOption::OPTIONAL)
            .help("random seed value");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        srand(parser.get<unsigned>("seed"));
        fmt::println("Note: seed is set to {}", parser.get<unsigned>("seed"));
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<Command> clearCmd() {
    auto cmd = make_unique<Command>("CLEAR");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("clear the console");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        cli.clearConsole();

        return CmdExecResult::DONE;
    };

    return cmd;
};
