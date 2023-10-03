/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define common commands for any CLI ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdlib>
#include <string>

#include "cli/cli.hpp"
#include "fmt/color.h"
#include "spdlog/common.h"
#include "util/usage.hpp"
#include "util/util.hpp"

namespace dvlab {

using namespace dvlab::argparse;

Command alias_cmd(CommandLineInterface& cli) {
    return {
        "alias",
        [](ArgumentParser& parser) {
            parser.description("alias a command to another name");

            parser.add_argument<std::string>("alias")
                .required(false)
                .help("the alias to add");

            parser.add_argument<std::string>("replace-str")
                .required(false)
                .help("the string to alias to");

            parser.add_argument<std::string>("-d", "--delete")
                .metavar("alias")
                .help("delete the alias");
        },

        [&cli](ArgumentParser const& parser) {
            if (parser.parsed("-d")) {
                if (parser.parsed("alias") || parser.parsed("replace-str")) {
                    fmt::println(stderr, "Error: cannot specify alias and replace-str when deleting alias!!");
                    return CmdExecResult::error;
                }
                if (!cli.remove_alias(parser.get<std::string>("-d"))) {
                    return CmdExecResult::error;
                }
                return CmdExecResult::done;
            }

            auto alias       = parser.get<std::string>("alias");
            auto replace_str = parser.get<std::string>("replace-str");

            if (std::ranges::any_of(alias, [](char ch) { return isspace(ch); })) {
                fmt::println(stderr, "Error: alias cannot contain whitespaces!!");
                return CmdExecResult::error;
            }
            if (cli.add_alias(alias, replace_str)) {
                return CmdExecResult::error;
            }

            return CmdExecResult::done;
        }};
}

Command help_cmd(CommandLineInterface& cli) {
    return {
        "help",
        [](ArgumentParser& parser) {
            parser.description("shows helping message to commands");

            parser.add_argument<std::string>("command")
                .default_value("")
                .nargs(NArgsOption::optional)
                .help("if specified, display help message to a command");
        },

        [&cli](ArgumentParser const& parser) {
            auto command = parser.get<std::string>("command");
            if (command.empty()) {
                cli.list_all_commands();
            } else {
                Command* e = cli.get_command(command);
                if (!e) {
                    fmt::println(stderr, "Error: illegal command!! ({})", command);
                    return CmdExecResult::error;
                }
                e->print_help();
            }
            return CmdExecResult::done;
        }};
}

Command quit_cmd(CommandLineInterface& cli) {
    return {"qquit",
            [](ArgumentParser& parser) {
                parser.description("quit Qsyn");

                parser.add_argument<bool>("-force")
                    .action(store_true)
                    .help("quit without reaffirming");
            },
            [&cli](ArgumentParser const& parser) {
                using namespace std::string_literals;
                if (parser.get<bool>("-force")) return CmdExecResult::quit;

                std::string const prompt = "Are you sure to quit (Yes/[No])? ";

                if (cli.listen_to_input(std::cin, prompt, {.allow_browse_history = false, .allow_tab_completion = false}) == CmdExecResult::quit) {
                    fmt::print("EOF [assumed Yes]");
                    return CmdExecResult::quit;
                }

                auto input = dvlab::str::tolower_string(dvlab::str::strip_leading_spaces(cli.get_read_buffer()));

                if (input.empty()) return CmdExecResult::done;

                return ("yes"s.starts_with(input))
                           ? CmdExecResult::quit
                           : CmdExecResult::done;  // not yet to quit
            }};
}

Command history_cmd(CommandLineInterface& cli) {
    return {"history",
            [](ArgumentParser& parser) {
                parser.description("print command history");
                parser.add_argument<size_t>("num")
                    .nargs(NArgsOption::optional)
                    .help("if specified, print the `num` latest command history");
            },
            [&cli](ArgumentParser const& parser) {
                if (parser.parsed("num")) {
                    cli.print_history(parser.get<size_t>("num"));
                } else {
                    cli.print_history();
                }
                return CmdExecResult::done;
            }};
}

Command dofile_cmd(CommandLineInterface& cli) {
    return {"dofile",
            [](ArgumentParser& parser) {
                parser.description("execute the commands in the dofile");

                parser.add_argument<std::string>("file")
                    .constraint(path_readable)
                    .help("path to a dofile, i.e., a list of Qsyn commands");

                parser.add_argument<std::string>("arguments")
                    .nargs(NArgsOption::zero_or_more)
                    .help("arguments to the dofile");
            },
            [&cli](ArgumentParser const& parser) {
                auto arguments = parser.get<std::vector<std::string>>("arguments");
                if (!cli.add_variables_from_dofiles(parser.get<std::string>("file"), arguments)) {
                    return CmdExecResult::error;
                }

                if (!cli.open_dofile(parser.get<std::string>("file"))) {
                    spdlog::error("cannot open file \"{}\"!!", parser.get<std::string>("file"));
                    return CmdExecResult::error;
                }

                return CmdExecResult::done;
            }};
}

Command usage_cmd() {
    return {"usage",
            [](ArgumentParser& parser) {
                parser.description("report the runtime and/or memory usage");

                auto mutex = parser.add_mutually_exclusive_group();

                mutex.add_argument<bool>("-all")
                    .action(store_true)
                    .help("print both time and memory usage");
                mutex.add_argument<bool>("-time")
                    .action(store_true)
                    .help("print time usage");
                mutex.add_argument<bool>("-memory")
                    .action(store_true)
                    .help("print memory usage");
            },
            [](ArgumentParser const& parser) {
                auto rep_all  = parser.get<bool>("-all");
                auto rep_time = parser.get<bool>("-time");
                auto rep_mem  = parser.get<bool>("-memory");

                if (!rep_all && !rep_time && !rep_mem) rep_all = true;

                if (rep_all) rep_time = true, rep_mem = true;

                dvlab::utils::Usage::report(rep_time, rep_mem);

                return CmdExecResult::done;
            }};
}

// Command verbose_cmd() {
//     return {"verbose",
//             [](ArgumentParser& parser) {
//                 parser.description("set verbose level to 0-9 (default: 3)");

//                 parser.add_argument<size_t>("level")
//                     .constraint([](size_t const& val) {
//                         if (val == 353 || (0 <= val && val <= 9)) return true;  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
//                         fmt::println(stderr, "Error: verbose level should be 0-9!!");
//                         return false;
//                     })
//                     .help("0: silent, 1-3: normal usage, 4-6: detailed info, 7-9: prolix debug info");
//             },

//             [](ArgumentParser const& parser) {
//                 VERBOSE = parser.get<size_t>("level");
//                 fmt::println("Note: verbose level is set to {}", VERBOSE);

//                 return CmdExecResult::done;
//             }};
// }

Command logger_cmd() {
    Command cmd{
        "logger",
        [](ArgumentParser& parser) {
            std::vector<std::string> log_levels = {"off", "critical", "error", "warn", "info", "debug", "trace"};
            parser.description("display and set the logger's status");

            auto parsers = parser.add_subparsers()
                               .help("subcommands for logger");
        },
        [](ArgumentParser const& /*parser*/) {
            fmt::println("Logger Level: {}", spdlog::level::to_string_view(spdlog::get_level()));
            std::vector<std::string> masked;
            std::vector<std::string> log_levels = {"critical", "error", "warn", "info", "debug", "trace"};
            // for (auto& level : log_levels) {
            //     if (LOGGER.is_masked(*Logger::str_to_log_level(level))) {
            //         masked.push_back(level);
            //     }
            // }

            // if (masked.size()) {
            //     fmt::println("Masked logging levels: {}", fmt::join(masked, ", "));
            // }

            return CmdExecResult::done;
        }};

    cmd.add_subcommand(
        {"test",
         [](ArgumentParser& parser) {
             parser.description("Test out logger setting");
         },
         [](ArgumentParser const& /*parser*/) {
             spdlog::log(spdlog::level::level_enum::off, "Regular printing (level `off`)");
             spdlog::critical("A log message with level `critical`");
             spdlog::error("A log message with level `error`");
             spdlog::warn("A log message with level `warn`");
             spdlog::info("A log message with level `info`");
             spdlog::debug("A log message with level `debug`");
             spdlog::trace("A log message with level `trace`");
             return CmdExecResult::done;
         }});

    cmd.add_subcommand(
        {"level",
         [](ArgumentParser& parser) {
             parser.description("set logger level");
             parser.add_argument<std::string>("level")
                 .constraint(choices_allow_prefix(std::vector<std::string>{"off", "critical", "error", "warn", "info", "debug", "trace"}))
                 .help("set log levels. Levels (ascending): off, critical, error, warn, info, debug, trace");
         },
         [](ArgumentParser const& parser) {
             auto level = spdlog::level::from_str(parser.get<std::string>("level"));
             spdlog::set_level(level);
             spdlog::info("Setting logger level to \"{}\"", spdlog::level::to_string_view(level));
             return CmdExecResult::done;
         }});

    // cmd.add_subcommand(
    //     {"history",
    //      [](ArgumentParser& parser) {
    //          parser.description("print logger history");
    //          parser.add_argument<size_t>("num_history")
    //              .nargs(NArgsOption::optional)
    //              .metavar("N")
    //              .help("print log history. If specified, print the lastest N logs");
    //      },
    //      [](ArgumentParser const& parser) {
    //          if (parser.parsed("num_history")) {
    //              LOGGER.print_logs(parser.get<size_t>("num_history"));
    //          } else {
    //              LOGGER.print_logs();
    //          }
    //          return CmdExecResult::done;
    //      }});

    // cmd.add_subcommand(
    //     {"mask",
    //      [](ArgumentParser& parser) {
    //          parser.description("set logger mask")
    //              .option_prefix("+-");
    //          auto add_filter_group = [&parser](std::string const& group_name) {
    //              auto group = parser.add_mutually_exclusive_group();
    //              group.add_argument<bool>("+" + group_name)
    //                  .action(store_true)
    //                  .help("unmask " + group_name + " logs");
    //              group.add_argument<bool>("-" + group_name)
    //                  .action(store_true)
    //                  .help("mask " + group_name + " logs");
    //          };

    //          std::vector<std::string> log_levels = {"fatal", "error", "warn", "info", "debug", "trace"};

    //          for (auto& group : log_levels) {
    //              if (group == "none") continue;
    //              add_filter_group(group);
    //          }
    //      },
    //      [](ArgumentParser const& parser) {
    //          using dvlab::Logger;

    //          std::vector<std::string> log_levels = {"fatal", "error", "warn", "info", "debug", "trace"};

    //          for (auto& group : log_levels) {
    //              auto level = Logger::str_to_log_level(group);
    //              assert(level.has_value());
    //              if (parser.parsed("+" + group)) {
    //                  LOGGER.unmask(*level);
    //                  LOGGER.debug("Unmasked logger level: {}", Logger::log_level_to_str(*level));
    //              } else if (parser.parsed("-" + group)) {
    //                  LOGGER.mask(*level);
    //                  LOGGER.debug("Masked logger level: {}", Logger::log_level_to_str(*level));
    //              }
    //          }
    //          return CmdExecResult::done;
    //      }});

    return cmd;
}

Command seed_cmd() {
    return {"seed",

            [](ArgumentParser& parser) {
                parser.description("set the random seed");

                parser.add_argument<unsigned>("seed")
                    .default_value(353)  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                    .nargs(NArgsOption::optional)
                    .help("random seed value");
            },

            [](ArgumentParser const& parser) {
                srand(parser.get<unsigned>("seed"));
                fmt::println("Note: seed is set to {}", parser.get<unsigned>("seed"));
                return CmdExecResult::done;
            }};
}

Command clear_cmd() {
    return {"clear",
            [](ArgumentParser& parser) {
                parser.description("clear the terminal");
            },

            [](ArgumentParser const& /*parser*/) {
                ::dvlab::detail::clear_terminal();
                return CmdExecResult::done;
            }};
};

bool add_cli_common_cmds(dvlab::CommandLineInterface& cli) {
    if (!(cli.add_command(alias_cmd(cli)) &&
          cli.add_command(quit_cmd(cli)) &&
          cli.add_command(history_cmd(cli)) &&
          cli.add_command(help_cmd(cli)) &&
          cli.add_command(dofile_cmd(cli)) &&
          cli.add_command(usage_cmd()) &&
          //   cli.add_command(verbose_cmd()) &&
          cli.add_command(seed_cmd()) &&
          cli.add_command(clear_cmd()) &&
          cli.add_command(logger_cmd()))) {
        spdlog::critical("Registering \"cli\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace dvlab
