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

#include "argparse/argGroup.hpp"
#include "cli/cli.hpp"
#include "util/usage.hpp"
#include "util/util.hpp"

using namespace std;
extern size_t verbose;
extern dvlab::utils::Usage usage;

using namespace ArgParse;

Command helpCmd();
Command quitCmd();
Command dofileCmd();
Command usageCmd();
Command verboseCmd();
Command seedCmd();
Command historyCmd();
Command clearCmd();
Command loggerCmd();

bool initCommonCmd() {
    if (!(cli.registerCommand("qquit", 2, quitCmd()) &&
          cli.registerCommand("history", 3, historyCmd()) &&
          cli.registerCommand("help", 3, helpCmd()) &&
          cli.registerCommand("dofile", 2, dofileCmd()) &&
          cli.registerCommand("usage", 5, usageCmd()) &&
          cli.registerCommand("verbose", 3, verboseCmd()) &&
          cli.registerCommand("seed", 4, seedCmd()) &&
          cli.registerCommand("clear", 5, clearCmd()) &&
          cli.registerCommand("logger", 3, loggerCmd()))) {
        logger.fatal("Registering \"cli\" commands fails... exiting");
        return false;
    }
    return true;
}

Command helpCmd() {
    return {
        "HELp",
        [](ArgumentParser& parser) {
            parser.description("shows helping message to commands");

            parser.addArgument<string>("command")
                .defaultValue("")
                .nargs(NArgsOption::OPTIONAL)
                .help("if specified, display help message to a command");
        },

        [](ArgumentParser const& parser) {
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
        }};
}

Command quitCmd() {
    return {
        "QQuit",
        [](ArgumentParser& parser) {
            parser.description("quit Qsyn");

            parser.addArgument<bool>("-force")
                .action(storeTrue)
                .help("quit without reaffirming");
        },
        [](ArgumentParser const& parser) {
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
        }};
}

Command historyCmd() {
    return {"history",
            [](ArgumentParser& parser) {
                parser.description("print command history");
                parser.addArgument<size_t>("num")
                    .nargs(NArgsOption::OPTIONAL)
                    .help("if specified, print the `num` latest command history");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("num")) {
                    cli.printHistory(parser.get<size_t>("num"));
                } else {
                    cli.printHistory();
                }
                return CmdExecResult::DONE;
            }};
}

Command dofileCmd() {
    return {"dofile",
            [](ArgumentParser& parser) {
                parser.description("execute the commands in the dofile");

                parser.addArgument<string>("file")
                    .constraint(path_readable)
                    .help("path to a dofile, i.e., a list of Qsyn commands");

                parser.addArgument<string>("arguments")
                    .nargs(NArgsOption::ZERO_OR_MORE)
                    .help("arguments to the dofile");
            },

            [](ArgumentParser const& parser) {
                auto arguments = parser.get<std::vector<string>>("arguments");
                if (!cli.saveVariables(parser.get<string>("file"), arguments)) {
                    return CmdExecResult::ERROR;
                }

                if (!cli.openDofile(parser.get<string>("file"))) {
                    logger.error("cannot open file \"{}\"!!", parser.get<std::string>("file"));
                    return CmdExecResult::ERROR;
                }

                return CmdExecResult::DONE;
            }};
}

Command usageCmd() {
    return {"usage",
            [](ArgumentParser& parser) {
                parser.description("report the runtime and/or memory usage");

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
            },
            [](ArgumentParser const& parser) {
                auto repAll = parser.get<bool>("-all");
                auto repTime = parser.get<bool>("-time");
                auto repMem = parser.get<bool>("-memory");

                if (!repAll && !repTime && !repMem) repAll = true;

                if (repAll) repTime = true, repMem = true;

                usage.report(repTime, repMem);

                return CmdExecResult::DONE;
            }};
}

Command verboseCmd() {
    return {"verbose",
            [](ArgumentParser& parser) {
                parser.description("set verbose level to 0-9 (default: 3)");

                parser.addArgument<size_t>("level")
                    .constraint([](size_t const& val) {
                        if (val == 353 || 0 <= val && val <= 9) return true;  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                        fmt::println(stderr, "Error: verbose level should be 0-9!!");
                        return false;
                    })
                    .help("0: silent, 1-3: normal usage, 4-6: detailed info, 7-9: prolix debug info");
            },

            [](ArgumentParser const& parser) {
                verbose = parser.get<size_t>("level");
                fmt::println("Note: verbose level is set to {}", verbose);

                return CmdExecResult::DONE;
            }};
}

Command loggerCmd() {
    Command cmd{
        "LOGger",
        [](ArgumentParser& parser) {
            vector<string> logLevels = {"none", "fatal", "error", "warning", "info", "debug", "trace"};
            parser.description("display and set the logger's status");

            auto parsers = parser.addSubParsers()
                               .help("subcommands for logger");
        },
        [](ArgumentParser const& parser) {
            using dvlab::utils::Logger;

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
        }};

    cmd.addSubCommand(
        {"test",
         [](ArgumentParser& parser) {
             parser.description("Test out logger setting");
         },
         [](ArgumentParser const& parser) {
             using namespace dvlab::utils;
             logger.fatal("Test fatal log");
             logger.error("Test error log");
             logger.warning("Test warning log");
             logger.info("Test info log");
             logger.debug("Test debug log");
             logger.trace("Test trace log");
             return CmdExecResult::DONE;
         }});

    cmd.addSubCommand(
        {"level",
         [](ArgumentParser& parser) {
             parser.description("set logger level");
             parser.addArgument<string>("level")
                 .constraint(choices_allow_prefix(vector<string>{"none", "fatal", "error", "warning", "info", "debug", "trace"}))
                 .help("set log levels. Levels (ascending): None, Fatal, Error, Warning, Info, Debug, Trace");
         },
         [](ArgumentParser const& parser) {
             using namespace dvlab::utils;
             auto level = Logger::str2LogLevel(parser.get<string>("level"));
             assert(level.has_value());
             logger.setLogLevel(*level);
             logger.debug("Setting logger level to {}", Logger::logLevel2Str(*level));
             return CmdExecResult::DONE;
         }});

    cmd.addSubCommand(
        {"history",
         [](ArgumentParser& parser) {
             parser.description("print logger history");
             parser.addArgument<size_t>("num_history")
                 .nargs(NArgsOption::OPTIONAL)
                 .metavar("N")
                 .help("print log history. If specified, print the lastest N logs");
         },
         [](ArgumentParser const& parser) {
             using dvlab::utils::Logger;
             if (parser.parsed("num_history")) {
                 logger.printLogs(parser.get<size_t>("num_history"));
             } else {
                 logger.printLogs();
             }
             return CmdExecResult::DONE;
         }});

    cmd.addSubCommand(
        {"mask",
         [](ArgumentParser& parser) {
             parser.description("set logger mask");
             parser.setOptionPrefix("+-");
             auto addFilterGroup = [&parser](std::string const& groupName) {
                 auto group = parser.addMutuallyExclusiveGroup();
                 group.addArgument<bool>("+" + groupName)
                     .action(storeTrue)
                     .help("unmask " + groupName + " logs");
                 group.addArgument<bool>("-" + groupName)
                     .action(storeTrue)
                     .help("mask " + groupName + " logs");
             };

             vector<string> logLevels = {"fatal", "error", "warning", "info", "debug", "trace"};

             for (auto& group : logLevels) {
                 if (group == "none") continue;
                 addFilterGroup(group);
             }
         },
         [](ArgumentParser const& parser) {
             using dvlab::utils::Logger;

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
         }});

    return cmd;
}

Command seedCmd() {
    return {"seed",

            [](ArgumentParser& parser) {
                parser.description("set the random seed");

                parser.addArgument<unsigned>("seed")
                    .defaultValue(353)  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                    .nargs(NArgsOption::OPTIONAL)
                    .help("random seed value");
            },

            [](ArgumentParser const& parser) {
                srand(parser.get<unsigned>("seed"));
                fmt::println("Note: seed is set to {}", parser.get<unsigned>("seed"));
                return CmdExecResult::DONE;
            }};
}

Command clearCmd() {
    return {"clear",
            [](ArgumentParser& parser) {
                parser.description("clear the terminal");
            },

            [](ArgumentParser const& parser) {
                cli.clearTerminal();
                return CmdExecResult::DONE;
            }};
};
