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

    cout << endl;
}

void CommandLineInterface::printHistory() const {
    printHistory(_history.size());
}

void CommandLineInterface::printHistory(size_t nPrint) const {
    assert(_tempCmdStored == false);
    if (_history.empty()) {
        cout << "Empty command history!!" << endl;
        return;
    }
    size_t s = _history.size();
    for (auto i = s - std::min(s, nPrint); i < s; ++i)
        cout << "   " << i << ": " << _history[i] << endl;
}

void CommandLineInterface::printPrompt() const {
    cout << _prompt << std::flush;
}

void CommandLineInterface::resetBufAndPrintPrompt() {
    _readBuf.clear();
    _cursorPosition = 0;
    _tabPressCount = 0;
    printPrompt();
}

void CommandLineInterface::beep() const {
    cout << (char)KeyCode::BEEP_CHAR;
}

void CommandLineInterface::clearConsole() const {
#ifdef _WIN32
    int result = system("cls");
#else
    int result = system("clear");
#endif
    if (result != 0) {
        cerr << "Error clearing the console!!" << endl;
    }
}