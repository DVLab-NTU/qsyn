/****************************************************************************
  FileName     [ cliParser.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define command parsing member functions for class CmdParser ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <fort.hpp>
#include <fstream>
#include <iostream>
#include <regex>
#include <thread>

#include "cli/cli.hpp"
#include "util/util.hpp"

using std::string, std::vector;
namespace fs = std::filesystem;

//----------------------------------------------------------------------
//    Member Function for class cmdParser
//----------------------------------------------------------------------
// return false if file cannot be opened
// Please refer to the comments in "DofileCmd::exec", cmdCommon.cpp
bool CommandLineInterface::openDofile(const std::string& dof) {
    constexpr size_t dofile_stack_limit = 1024;
    if (this->stop_requested()) {
        return false;
    }
    if (_dofileStack.size() >= dofile_stack_limit) {
        fmt::println(stderr, "Error: dofile stack overflow ({})!!", dofile_stack_limit);
        return false;
    }

    _dofileStack.push(std::move(std::ifstream(dof)));

    if (!_dofileStack.top().is_open()) {
        closeDofile();
        return false;
    }
    return true;
}

void CommandLineInterface::closeDofile() {
    assert(_dofileStack.size());
    _dofileStack.pop();
}

// Return false if registration fails
bool CommandLineInterface::regCmd(const string& cmd, unsigned nCmp, std::unique_ptr<CmdExec>&& e) {
    // Make sure cmd hasn't been registered and won't cause ambiguity
    string str = cmd;
    unsigned s = str.size();
    if (!e->initialize()) return false;
    if (s < nCmp) return false;
    while (true) {
        if (getCmd(str)) return false;
        if (s == nCmp) break;
        str.resize(--s);
    }

    assert(str.size() == nCmp);  // str is now mandCmd
    string& mandCmd = str;
    for (unsigned i = 0; i < nCmp; ++i)
        mandCmd[i] = toupper(mandCmd[i]);
    string optCmd = cmd.substr(nCmp);
    assert(e != 0);
    e->setOptCmd(optCmd);

    // insert (mandCmd, e) to _cmdMap; return false if insertion fails.
    return (_cmdMap.insert(CmdRegPair(mandCmd, std::move(e)))).second;
}

void CommandLineInterface::sigintHandler(int signum) {
    if (_currCmd.has_value()) {
        // there is an executing command
        _currCmd->request_stop();
        fmt::println("Command Interrupted");
    } else {
        // receiving inputs
        fmt::print("\n");
        resetBufAndPrintPrompt();
    }
}

// Return false on "quit" or if exception happens
CmdExecResult
CommandLineInterface::executeOneLine() {
    bool newCmd = false;
    while (_dofileStack.size() && _dofileStack.top().eof()) closeDofile();

    if (_dofileStack.size())
        newCmd = readCmd(_dofileStack.top());
    else {
        newCmd = readCmd(std::cin);
    }

    // execute the command
    if (!newCmd) return CmdExecResult::NOP;

    std::atomic<CmdExecResult> result;

    while (_commandQueue.size()) {
        auto [e, option] = parseOneCommandFromQueue();

        if (e == nullptr) continue;

        _currCmd = jthread::jthread(
            [this, &e = e, &option = option, &result]() {
                result = e->exec(option);
            });

        assert(_currCmd.has_value());

        _currCmd->join();

        if (this->stop_requested()) {
            fmt::println(stderr, "Command interrupted");
            while (_commandQueue.size()) _commandQueue.pop();
            return CmdExecResult::INTERRUPTED;
        }

        _currCmd = std::nullopt;
    }

    return result;
}

std::pair<CmdExec*, std::string>
CommandLineInterface::parseOneCommandFromQueue() {
    assert(_tempCmdStored == false);
    string buffer = _commandQueue.front();
    _commandQueue.pop();

    assert(buffer[0] != '\0' && buffer[0] != ' ');

    string cmd;
    size_t n = myStrGetTok(buffer, cmd);
    CmdExec* e = getCmd(cmd);
    string option;
    if (!e) {
        fmt::println(stderr, "Illegal command!! ({})", cmd);
    } else if (n != string::npos) {
        option = buffer.substr(n);
    }
    return {e, option};
}

string CommandLineInterface::replaceVariableKeysWithValues(string const& str) const {
    // if `str` contains the some dollar sign '$',
    // try to convert it into variable
    // unless it is preceded by '\'.

    // Variables are in the form of `$NAME` or `${NAME}`,
    // where the name should consists of only alphabets, numbers,
    // and the underscore '_'.

    // If curly braces are used (${NAME}),
    // the text inside the curly braces is the variable name.

    // If otherwise no curly braces are used ($NAME),
    // the variable name is until some illegal characters for a name appears.

    // if a variable is existent, replace the $NAME or ${NAME} syntax with their value. Otherwise, replace the syntax with an empty string

    // e.g., suppose foo_bar=apple, foo=banana
    //       "$foo_bar"     --> "apple"
    //       "$foo.bar"     --> "banana.bar"
    //       "${foo}_bar"   --> "banana_bar"
    //       "foo_$bar"     --> "foo_"
    //       "${foo}${bar}" --> "banana"

    // optional: if inside ${NAME} is an illegal name string,
    // warn the user.

    std::vector<std::tuple<size_t, size_t, string>> to_replace;
    // \S means non-whitespace character
    static std::regex const var_without_braces(R"(\$[a-zA-Z0-9_]+)");
    static std::regex const var_with_braces(R"(\$\{\S+\})");
    for (auto re : {var_without_braces, var_with_braces}) {
        std::smatch matches;
        std::regex_search(str, matches, re);
        for (size_t i = 0; i < matches.size(); ++i) {
            string var = matches[i];
            // tell if it is a curly brace variable or not
            bool is_brace = var[1] == '{';
            string var_key = is_brace ? var.substr(2, var.length() - 3) : var.substr(1);

            bool is_defined = _variables.contains(var_key);
            string val = is_defined ? _variables.at(var_key) : "";

            if (is_brace && !is_defined) {
                for (auto ch : var_key) {
                    if (isalnum(ch) || ch == '_') {
                        continue;
                    }
                    fmt::println(stderr, "Warning: variable name `{}` is illegal!!", var_key);
                    break;
                }
            }

            size_t pos = matches.position(i);
            if (isEscapedChar(str, pos)) {
                continue;
            }
            to_replace.emplace_back(pos, var.length(), val);
        }
    }

    size_t cursor = 0;
    string result = "";
    for (auto [pos, len, val] : to_replace) {
        result += str.substr(cursor, pos - cursor);
        result += val;
        cursor = pos + len;
    }
    result += str.substr(cursor);

    // return a string with all variables substituted with their value.
    return result;
}

// cmd is a copy of the original input
//
// return the corresponding CmdExec* if "cmd" matches any command in _cmdMap
// return 0 if not found.
//
// Please note:
// ------------
// 1. The mandatory part of the command string (stored in _cmdMap) must match
// 2. The optional part can be partially omitted.
// 3. All string comparison are "case-insensitive".
//
CmdExec*
CommandLineInterface::getCmd(string cmd) {
    CmdExec* e = nullptr;

    for (unsigned i = 0; i < cmd.size(); ++i) {
        cmd[i] = toupper(cmd[i]);
        string check = cmd.substr(0, i + 1);
        if (_cmdMap.find(check) != _cmdMap.end())
            e = _cmdMap[check].get();
        if (e != nullptr) {
            string optCheck = cmd.substr(i + 1);
            if (e->checkOptCmd(optCheck))
                return e;  // match found!!
            else
                e = nullptr;
        }
    }
    return e;
}

//----------------------------------------------------------------------
//    Member Function for class CmdExec
//----------------------------------------------------------------------

// Called by "getCmd()"
// Check if "check" is a matched substring of "_optCmd"...
// if not, return false.
//
// Perform case-insensitive checks
//
bool CmdExec::checkOptCmd(const string& check) const {
    if (check.size() > _optCmd.size()) return false;
    for (unsigned i = 0, n = _optCmd.size(); i < n; ++i) {
        if (!check[i]) return true;
        char ch1 = tolower(_optCmd[i]);
        char ch2 = tolower(check[i]);
        if (ch1 != ch2) return false;
    }
    return true;
}
