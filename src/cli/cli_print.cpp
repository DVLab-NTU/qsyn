/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define printing functions of class dvlab::CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <ranges>

#include "./cli.hpp"

namespace dvlab {

/**
 * @brief print a summary of all commands
 *
 */
void dvlab::CommandLineInterface::list_all_commands() const {
    auto cmd_range = _commands | std::views::keys;
    std::vector<std::string> cmd_vec(cmd_range.begin(), cmd_range.end());
    std::ranges::sort(cmd_vec);
    for (auto const& cmd : cmd_vec)
        _commands.at(cmd)->print_summary();

    fmt::println("");
}

/**
 * @brief print all CLI history
 *
 */
void dvlab::CommandLineInterface::print_history() const {
    print_history(_history.size());
}

/**
 * @brief print the last nPrint commands in CLI history
 *
 * @param nPrint
 */
void dvlab::CommandLineInterface::print_history(size_t n_print) const {
    assert(_temp_command_stored == false);
    if (_history.empty()) {
        fmt::println(("Empty command history!!"));
        return;
    }
    size_t s = _history.size();
    for (auto i = s - std::min(s, n_print); i < s; ++i) {
        fmt::println("{:>4}: {}", i, _history[i]);
    }
}

void dvlab::CommandLineInterface::_print_prompt() const {
    fmt::print("{}", _command_prompt);
    fflush(stdout);
}

}  // namespace dvlab