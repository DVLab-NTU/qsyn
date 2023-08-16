/****************************************************************************
  FileName     [ cliRead.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Read the command from the standard input or dofile ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <cassert>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>

#include "cli/cli.hpp"
#include "cli/cliCharDef.hpp"

using namespace std;

//----------------------------------------------------------------------
//    Member Function for class CmdParser
//----------------------------------------------------------------------

void CommandLineInterface::askForUserInput(std::istream& istr) {
    using namespace KeyCode;

    while (true) {
        int keycode = getChar(istr);

        if (istr.eof()) return;

        if (keycode == INPUT_END_KEY) {
            fmt::println("\nquit");
            std::exit(0);
        }

        switch (keycode) {
            case NEWLINE_KEY:
                return;
            case LINE_BEGIN_KEY:
            case HOME_KEY:
                moveCursor(0);
                break;
            case LINE_END_KEY:
            case END_KEY:
                moveCursor(_readBuf.size());
                break;
            case BACK_SPACE_KEY:
                if (moveCursor(_cursorPosition - 1))
                    deleteChar();
                break;
            case DELETE_KEY:
                deleteChar();
                break;
            case CLEAR_CONSOLE_KEY:
                clearConsole();
                fmt::print("\n");
                resetBufAndPrintPrompt();
                break;
            case ARROW_UP_KEY:
                moveToHistory(_historyIdx - 1);
                break;
            case ARROW_DOWN_KEY:
                moveToHistory(_historyIdx + 1);
                break;
            case ARROW_RIGHT_KEY:
                moveCursor(_cursorPosition + 1);
                break;
            case ARROW_LEFT_KEY:
                moveCursor((int)_cursorPosition - 1);
                break;
            case PG_UP_KEY:
                moveToHistory(_historyIdx - PG_OFFSET);
                break;
            case PG_DOWN_KEY:
                moveToHistory(_historyIdx + PG_OFFSET);
                break;
            case TAB_KEY: {
                ++_tabPressCount;
                matchAndComplete(stripLeadingWhitespaces(_readBuf.substr(0, _cursorPosition)));
                break;
            }
            case INSERT_KEY:  // not yet supported; fall through to UNDEFINE
            case UNDEFINED_KEY:
                beep();
                break;
            default:  // printable character
                insertChar(char(keycode));
                break;
        }
    }
}

bool CommandLineInterface::readCmd(istream& istr) {
    resetBufAndPrintPrompt();

    this->askForUserInput(istr);

    bool added = addUserInputToHistory();

    if (added) {
        auto stripped = stripQuotes(_history.back()).value_or("");

        stripped = replaceVariableKeysWithValues(stripped);
        std::vector<std::string> tokens = split(stripped, ";");

        if (tokens.size()) {
            // concat tokens with '\;' to a single token
            for (auto itr = next(tokens.rbegin()); itr != tokens.rend(); ++itr) {
                string& currToken = *itr;
                string& nextToken = *prev(itr);

                if (currToken.ends_with('\\') && !currToken.ends_with("\\\\")) {
                    currToken.back() = ';';
                    currToken += nextToken;
                    nextToken = "";
                }
            }
            erase_if(tokens, [](std::string const& token) { return token == ""; });
            std::ranges::for_each(tokens, [this](std::string& token) { _commandQueue.push(stripWhitespaces(token)); });
        }

        fmt::print("\n");
        fflush(stdout);
    }

    return added;
}

// This function moves _readBufPtr to the "ptr" pointer
// It is used by left/right arrowkeys, home/end, etc.
//
// Suggested steps:
// 1. Make sure ptr is within [_readBuf, _readBufEnd].
//    If not, make a beep sound and return false. (DON'T MOVE)
// 2. Move the cursor to the left or right, depending on ptr
// 3. Update _readBufPtr accordingly. The content of the _readBuf[] will
//    not be changed
//
// [Note] This function can also be called by other member functions below
//        to move the _readBufPtr to proper position.
bool CommandLineInterface::moveCursor(int idx) {
    if (idx < 0 || (size_t)idx > _readBuf.size()) {
        beep();
        return false;
    }

    // move left
    if (_cursorPosition > (size_t)idx) {
        fmt::print("{}", string(_cursorPosition - idx, '\b'));
    }

    // move right
    if (_cursorPosition < (size_t)idx) {
        fmt::print("{}", _readBuf.substr(_cursorPosition, idx - _cursorPosition));
    }
    _cursorPosition = idx;
    return true;
}

// [Notes]
// 1. Delete the char at _readBufPtr
// 2. mybeep() and return false if at _readBufEnd
// 3. Move the remaining string left for one character
// 4. The cursor should stay at the same position
// 5. Remember to update _readBufEnd accordingly.
// 6. Don't leave the tailing character.
// 7. Call "moveBufPtr(...)" if needed.
//
// For example,
//
// cmd> This is the command
//              ^                (^ is the cursor position)
//
// After calling deleteChar()---
//
// cmd> This is he command
//              ^
//
bool CommandLineInterface::deleteChar() {
    if (_cursorPosition == _readBuf.size()) {
        beep();
        return false;
    }
    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.
    fmt::print("{}", _readBuf.substr(_cursorPosition + 1));  // move cursor to the end
    fmt::print(" \b");                                       // get rid of the last character

    _readBuf.erase(_cursorPosition, 1);

    int idx = _cursorPosition;
    _cursorPosition = _readBuf.size();  // before moving cursor, reflect the change in actual cursor location
    moveCursor(idx);                    // move the cursor back to where it should be
    return true;
}

// 1. Insert character 'ch' for "repeat" times at _readBufPtr
// 2. Move the remaining string right for "repeat" characters
// 3. The cursor should move right for "repeats" positions afterwards
// 4. Default value for "repeat" is 1. You should assert that (repeat >= 1).
//
// For example,
//
// cmd> This is the command
//              ^                (^ is the cursor position)
//
// After calling insertChar('k', 3) ---
//
// cmd> This is kkkthe command
//                 ^
//
void CommandLineInterface::insertChar(char ch) {
    _readBuf.insert(_cursorPosition, 1, ch);
    fmt::print("{}", _readBuf.substr(_cursorPosition));
    int idx = _cursorPosition + 1;
    _cursorPosition = _readBuf.size();
    moveCursor(idx);
}

// 1. Delete the line that is currently shown on the screen
// 2. Reset _readBufPtr and _readBufEnd to _readBuf
// 3. Make sure *_readBufEnd = 0
//
// For example,
//
// cmd> This is the command
//              ^                (^ is the cursor position)
//
// After calling deleteLine() ---
//
// cmd>
//      ^
//
void CommandLineInterface::deleteLine() {
    moveCursor(_readBuf.size());
    fmt::print("{}", string(_cursorPosition, '\b'));
    fmt::print("{}", string(_cursorPosition, ' '));
    fmt::print("{}", string(_cursorPosition, '\b'));
    _readBuf.clear();
}

// Reprint the current command to a newline
// cursor should be restored to the original location
void CommandLineInterface::reprintCmd() {
    fmt::print("\n");

    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.
    int idx = _cursorPosition;
    _cursorPosition = _readBuf.size();  // before moving cursor, reflect the change in actual cursor location
    printPrompt();
    fmt::print("{}", _readBuf);
    moveCursor(idx);  // move the cursor back to where it should be
}

// This functions moves _historyIdx to index and display _history[index]
// on the screen.
//
// Need to consider:
// If moving up... (i.e. index < _historyIdx)
// 1. If already at top (i.e. _historyIdx == 0), beep and do nothing.
// 2. If at bottom, temporarily record _readBuf to history.
//    (Do not remove spaces, and set _tempCmdStored to "true")
// 3. If index < 0, let index = 0.
//
// If moving down... (i.e. index > _historyIdx)
// 1. If already at bottom, beep and do nothing
// 2. If index >= _history.size(), let index = _history.size() - 1.
//
// Assign _historyIdx to index at the end.
//
// [Note] index should not = _historyIdx
//
void CommandLineInterface::moveToHistory(int index) {
    if (index < _historyIdx) {  // move up
        if (_historyIdx == 0) {
            beep();
            return;
        }
        if (size_t(_historyIdx) == _history.size()) {  // mv away from new str
            _tempCmdStored = true;
            _history.emplace_back(_readBuf);
        } else if (_tempCmdStored &&  // the last _history is a stored temp cmd
                   size_t(_historyIdx) == size_t(_history.size() - 1))
            _history.back() = _readBuf;  // => update it
        if (index < 0)
            index = 0;
    } else if (index > _historyIdx) {  // move down
        if ((_tempCmdStored &&
             (size_t(_historyIdx) == size_t(_history.size() - 1))) ||
            (!_tempCmdStored && (size_t(_historyIdx) == _history.size()))) {
            beep();
            return;
        }
        if (size_t(index) >= size_t(_history.size() - 1))
            index = int(_history.size() - 1);
    } else                             // index == _historyIdx
        assert(index != _historyIdx);  // must fail!!

    _historyIdx = index;
    retrieveHistory();
}

/**
 * @brief Add the command in buffer to _history.
 *        This function trim the comment, leading/trailing whitespace of the entered comments
 *
 */
bool CommandLineInterface::addUserInputToHistory() {
    size_t argumentTagPos = _readBuf.find("//!ARGS");
    if (argumentTagPos == 0) {
        saveArgumentsInVariables(_readBuf);
    }

    if (_tempCmdStored) {
        _history.pop_back();
        _tempCmdStored = false;
    }

    string cmd = stripWhitespaces(stripComments(_readBuf));

    if (cmd.size()) {
        _history.emplace_back(cmd);
    }

    _historyIdx = int(_history.size());

    return cmd.size() > 0;
}

// may exit if the check fails
void CommandLineInterface::saveArgumentsInVariables(std::string const& str) {
    // parse the string
    // "//!ARGS n <ARG1> <ARG2> ... <ARGn>"
    // and check if for all k = 1 to n,
    // _variables[to_string(k)] is mapped to a valid value

    // To enable keyword arguments, also map the names <ARGk>
    // to _variables[to_string(k)]

    std::istringstream iss(str);
    std::string token;
    iss >> token;  // skip the first token "//!ARGS"
    assert(token == "//!ARGS");

    regex validVariableName("[a-zA-Z_][a-zA-Z0-9_]*");
    std::vector<std::string> keys;
    while (iss >> token) {
        if (!regex_match(token, validVariableName)) {
            fmt::print(stderr, "\n");
            fmt::println(stderr, "Error: invalid argument name \"{}\" in \"//!ARGS\" directive", token);
            std::exit(-1);
        }
        keys.emplace_back(token);
    }

    if (_arguments.size() != keys.size()) {
        fmt::print(stderr, "\n");
        fmt::println(stderr, "Error: wrong number of arguments provided, expected {} but got {}!!", keys.size(), _arguments.size());
        std::exit(-1);
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        _variables.emplace(keys[i], _arguments[i]);
    }
}

// 1. Replace current line with _history[_historyIdx] on the screen
// 2. Set _readBufPtr and _readBufEnd to end of line
//
// [Note] Do not change _history.size().
//
void CommandLineInterface::retrieveHistory() {
    deleteLine();
    _readBuf = _history[_historyIdx];
    fmt::print("{}", _readBuf);
    _cursorPosition = _history[_historyIdx].size();
}
