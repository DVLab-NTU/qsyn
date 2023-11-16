/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define common commands for any CLI ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "fmt/color.h"
#include "spdlog/common.h"
#include "util/usage.hpp"

namespace std {
extern istream cin;
}

namespace dvlab {

using namespace dvlab::argparse;

Command alias_cmd(CommandLineInterface& cli) {
    return {
        "alias",
        [](ArgumentParser& parser) {
            parser.description("alias command string to another name");

            parser.add_argument<std::string>("alias")
                .required(false)
                .help("the alias to add");

            parser.add_argument<std::string>("replace-str")
                .required(false)
                .help("the string to alias to");

            parser.add_argument<std::string>("-d", "--delete")
                .metavar("alias")
                .help("delete the alias");

            parser.add_argument<bool>("-l", "--list")
                .action(store_true)
                .help("list all aliases");
        },

        [&cli](ArgumentParser const& parser) {
            if (parser.parsed("-l")) {
                cli.list_all_aliases();
                return CmdExecResult::done;
            }
            if (parser.parsed("-d")) {
                if (parser.parsed("alias") || parser.parsed("replace-str")) {
                    fmt::println(stderr, "Error: cannot specify replacement string when deleting alias!!");
                    return CmdExecResult::error;
                }
                if (!cli.remove_alias(parser.get<std::string>("-d"))) {
                    return CmdExecResult::error;
                }
                return CmdExecResult::done;
            }

            if (!(parser.parsed("alias") && parser.parsed("replace-str"))) {
                fmt::println(stderr, "Error: alias and replacement string must be specified!!");
                return CmdExecResult::error;
            }

            auto alias       = parser.get<std::string>("alias");
            auto replace_str = parser.get<std::string>("replace-str");

            if (cli.add_alias(alias, replace_str)) {
                return CmdExecResult::error;
            }

            return CmdExecResult::done;
        }};
}

Command set_variable_cmd(CommandLineInterface& cli) {
    return {
        "set",
        [](ArgumentParser& parser) {
            parser.description("set a variable");

            parser.add_argument<std::string>("variable")
                .required(false)
                .help("the variable to set");

            parser.add_argument<std::string>("value")
                .required(false)
                .help("the value to set");

            parser.add_argument<std::string>("-d", "--delete")
                .metavar("variable")
                .help("delete the variable");

            parser.add_argument<bool>("-l", "--list")
                .action(store_true)
                .help("list all variables");
        },

        [&cli](ArgumentParser const& parser) {
            if (parser.parsed("-l")) {
                cli.list_all_variables();
                return CmdExecResult::done;
            }

            if (parser.parsed("-d")) {
                if (parser.parsed("variable") || parser.parsed("value")) {
                    fmt::println(stderr, "Error: cannot specify values when deleting variable!!");
                    return CmdExecResult::error;
                }
                if (!cli.remove_variable(parser.get<std::string>("-d"))) {
                    return CmdExecResult::error;
                }
                return CmdExecResult::done;
            }

            if (!(parser.parsed("variable") && parser.parsed("value"))) {
                fmt::println(stderr, "Error: variable and value must be specified!!");
                return CmdExecResult::error;
            }

            auto variable = parser.get<std::string>("variable");
            auto value    = parser.get<std::string>("value");

            if (std::ranges::any_of(variable, [](char ch) { return isspace(ch); })) {
                fmt::println(stderr, "Error: variable cannot contain whitespaces!!");
                return CmdExecResult::error;
            }

            if (!cli.add_variable(variable, value)) {
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
                return CmdExecResult::done;
            }

            // this also handles the case when `command` is an alias to itself

            auto alias_replacement_string = cli.get_alias_replacement_string(command);
            if (alias_replacement_string.has_value()) {
                fmt::println("`{}` is an alias to `{}`.", command, alias_replacement_string.value());
                fmt::println("Showing help message to `{}`:\n", alias_replacement_string.value());
                command = cli.get_first_token(alias_replacement_string.value());
            }

            auto e = cli.get_command(command);
            if (e) {
                e->print_help();
                return CmdExecResult::done;
            }

            fmt::println(stderr, "Error: illegal command or alias!! ({})", command);
            return CmdExecResult::error;
        }};
}

Command quit_cmd(CommandLineInterface& cli) {
    return {"quit",
            [](ArgumentParser& parser) {
                parser.description("quit qsyn");

                parser.add_argument<bool>("-force")
                    .action(store_true)
                    .help("quit without reaffirming");
            },
            [&cli](ArgumentParser const& parser) {
                using namespace std::string_literals;
                if (parser.get<bool>("-force")) return CmdExecResult::quit;

                std::string const prompt = "Are you sure you want to exit (Yes/[No])? ";

                auto [exec_result, input] = cli.listen_to_input(std::cin, prompt, {.allow_browse_history = false, .allow_tab_completion = false});
                if (exec_result == CmdExecResult::quit) {
                    fmt::print("EOF [assumed Yes]");
                    return CmdExecResult::quit;
                }

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

Command source_cmd(CommandLineInterface& cli) {
    return {"source",
            [](ArgumentParser& parser) {
                parser.description("execute the commands in the dofile");

                parser.add_argument<bool>("-q", "--quiet")
                    .action(store_true)
                    .help("suppress the echoing of commands");
                parser.add_argument<std::string>("file")
                    .constraint(path_readable)
                    .help("path to a dofile, i.e., a list of qsyn commands");

                parser.add_argument<std::string>("arguments")
                    .nargs(NArgsOption::zero_or_more)
                    .help("arguments to the dofile");
            },
            [&cli](ArgumentParser const& parser) {
                auto arguments = parser.get<std::vector<std::string>>("arguments");
                return cli.source_dofile(parser.get<std::string>("file"), arguments, !parser.get<bool>("--quiet"));
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

Command logger_cmd() {
    Command cmd{
        "logger",
        [](ArgumentParser& parser) {
            static auto const log_levels = std::vector<std::string>{"off", "critical", "error", "warning", "info", "debug", "trace"};
            parser.description("display and set the logger's status");

            auto parsers = parser.add_subparsers()
                               .help("subcommands for logger");
        },
        [](ArgumentParser const& /*parser*/) {
            fmt::println("Logger Level: {}", spdlog::level::to_string_view(spdlog::get_level()));

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
             spdlog::warn("A log message with level `warning`");
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
                 .constraint(choices_allow_prefix(std::vector<std::string>{"off", "critical", "error", "warning", "info", "debug", "trace"}))
                 .help("set log levels. Levels (ascending): off, critical, error, warning, info, debug, trace");
         },
         [](ArgumentParser const& parser) {
             auto level = spdlog::level::from_str(parser.get<std::string>("level"));
             spdlog::set_level(level);
             spdlog::info("Setting logger level to \"{}\"", spdlog::level::to_string_view(level));
             return CmdExecResult::done;
         }});

    return cmd;
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
          cli.add_alias("unalias", "alias -d") &&
          cli.add_command(set_variable_cmd(cli)) &&
          cli.add_alias("unset", "set -d") &&
          cli.add_command(quit_cmd(cli)) &&
          cli.add_alias("q", "quit") &&
          cli.add_alias("exit", "quit") &&
          cli.add_command(history_cmd(cli)) &&
          cli.add_command(help_cmd(cli)) &&
          cli.add_command(source_cmd(cli)) &&
          cli.add_command(usage_cmd()) &&
          cli.add_command(clear_cmd()) &&
          cli.add_command(logger_cmd()))) {
        spdlog::critical("Registering \"cli\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace dvlab
