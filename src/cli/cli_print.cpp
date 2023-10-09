/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define printing functions of class dvlab::CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <ranges>

#include "./cli.hpp"
#include "tl/enumerate.hpp"

namespace dvlab {

/**
 * @brief print a summary of all commands
 *
 */
void dvlab::CommandLineInterface::list_all_commands() const {
    auto cmd_range = _commands | std::views::keys;
    std::vector<std::string> cmd_vec(std::begin(cmd_range), std::end(cmd_range));
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
    for (auto const& [i, history] : _history | std::views::drop(_history.size() - n_print) | tl::views::enumerate) {
        fmt::println("{:>4}: {}", i, history);
    }
}

void dvlab::CommandLineInterface::_print_prompt() const {
    fmt::print("{}", _command_prompt);
    fflush(stdout);
}

}  // namespace dvlab