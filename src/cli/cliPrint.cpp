/****************************************************************************
  FileName     [ cliPrint.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define printing functions of class CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./cli.hpp"

using namespace std;

void CommandLineInterface::printHelps() const {
    for (const auto& mi : _cmdMap)
        mi.second->summary();

    fmt::print("\n");
}

void CommandLineInterface::printHistory() const {
    printHistory(_history.size());
}

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

void CommandLineInterface::resetBufAndPrintPrompt() {
    _readBuf.clear();
    _cursorPosition = 0;
    _tabPressCount = 0;
    printPrompt();
}

void CommandLineInterface::beep() const {
    fmt::print("{}", (char)KeyCode::BEEP_CHAR);
}

void CommandLineInterface::clearConsole() const {
#ifdef _WIN32
    int result = system("cls");
#else
    int result = system("clear");
#endif
    if (result != 0) {
        fmt::println(stderr, "Error clearing the console!!");
    }
}