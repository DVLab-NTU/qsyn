/****************************************************************************
  FileName     [ cliPrint.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define printing functions of class CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./cli.hpp"

using namespace std;

/**
 * @brief print a summary of all commands
 *
 */
void CommandLineInterface::listAllCommands() const {
    for (const auto& cmd : _cmdMap | std::views::values)
        cmd->summary();

    fmt::print("\n");
}

/**
 * @brief print all CLI history
 *
 */
void CommandLineInterface::printHistory() const {
    printHistory(_history.size());
}

/**
 * @brief print the last nPrint commands in CLI history
 *
 * @param nPrint
 */
void CommandLineInterface::printHistory(size_t nPrint) const {
    assert(_tempCmdStored == false);
    if (_history.empty()) {
        fmt::println(("Empty command history!!"));
        return;
    }
    size_t s = _history.size();
    for (auto i = s - std::min(s, nPrint); i < s; ++i) {
        fmt::println("{:>4}: {}", i, _history[i]);
    }
}

void CommandLineInterface::printPrompt() const {
    fmt::print("{}", _prompt);
    fflush(stdout);
}

void CommandLineInterface::beep() const {
    fmt::print("{}", (char)KeyCode::BEEP_CHAR);
}

void CommandLineInterface::clearTerminal() const {
#ifdef _WIN32
    int result = system("cls");
#else
    int result = system("clear");
#endif
    if (result != 0) {
        fmt::println(stderr, "Error clearing the terminal!!");
    }
}