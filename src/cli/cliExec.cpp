/****************************************************************************
  FileName     [ cliParser.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define command parsing member functions for class CmdParser ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <atomic>
#include <cassert>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <fort.hpp>
#include <memory>
#include <regex>
#include <thread>

#include "cli/cli.hpp"
#include "util/util.hpp"

using std::string, std::vector;

std::string const CommandLineInterface::_specialChars = "\"\' ;$";

//----------------------------------------------------------------------
//    Member Function for class cmdParser
//----------------------------------------------------------------------

/**
 * @brief open a dofile and push it to the dofile stack.
 *
 * @param filepath the file to be opened
 * @return true
 * @return false
 */
bool CommandLineInterface::openDofile(const std::string& filepath) {
    constexpr size_t dofile_stack_limit = 1024;
    if (this->stopRequested()) {
        return false;
    }
    if (_dofileStack.size() >= dofile_stack_limit) {
        logger.error("dofile stack overflow ({})!!", dofile_stack_limit);
        return false;
    }

    _dofileStack.push(std::move(std::ifstream(filepath)));

    if (!_dofileStack.top().is_open()) {
        closeDofile();
        return false;
    }
    return true;
}

/**
 * @brief close the top dofile in the dofile stack.
 *
 */
void CommandLineInterface::closeDofile() {
    assert(_dofileStack.size());
    _dofileStack.pop();
}

/**
 * @brief register a command to the CLI.
 *
 * @param name the command name
 * @param nMandChars the number of characters to be matched.
 * @param cmd the command to be registered
 * @return true
 * @return false
 */
bool CommandLineInterface::registerCommand(const string& name, unsigned nReqChars, Command cmd) {
    // Make sure cmd hasn't been registered and won't cause ambiguity
    string str = name;
    unsigned s = str.size();
    if (s < nReqChars) {
        logger.error("Command name `{}` is shorter than number of required characters ({})!!", name, nReqChars);
        return false;
    }
    while (true) {
        if (getCommand(str)) {
            logger.error("Command name `{}` conflicts with existing command `{}`!!", name, str);
            return false;
        }
        if (s == nReqChars) break;
        str.resize(--s);
    }

    if (!cmd.initialize(nReqChars)) {
        logger.error("Failed to initialize command `{}`!!", name);
        return false;
    }

    assert(str.size() == nReqChars);  // str is now mandCmd
    string& mandCmd = str;

    string optCmd = name.substr(nReqChars);
    cmd.setOptCmd(optCmd);

    // insert (mandCmd, e) to _cmdMap; return false if insertion fails.
    return (_cmdMap.emplace(mandCmd, std::make_unique<Command>(std::move(cmd)))).second;
}

/**
 * @brief handle the SIGINT signal. Wrap the handler in a static function so that it can be passed to the signal function.
 *
 * @param signum
 */
void CommandLineInterface::sigintHandler(int signum) {
    if (_listeningForInputs) {
        resetBuffer();
        fmt::print("\n");
        printPrompt();
    } else if (_currCmd.has_value()) {
        // there is an executing command
        _currCmd->request_stop();
    } else {
        logger.fatal("Failed to handle the SIGINT signal. Exiting...");
        exit(signum);
    }
}

/**
 * @brief execute one line of commands.
 *
 * @return CmdExecResult
 */
CmdExecResult
CommandLineInterface::executeOneLine() {
    while (_dofileStack.size() && _dofileStack.top().eof()) closeDofile();
    if (auto result = (_dofileStack.size() ? readOneLine(_dofileStack.top()) : readOneLine(std::cin)); result != CmdExecResult::DONE) {
        if (_dofileStack.empty() && std::cin.eof()) return CmdExecResult::QUIT;
        return result;
    }

    // execute the command

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

        if (this->stopRequested()) {
            logger.warning("Command interrupted");
            while (_commandQueue.size()) _commandQueue.pop();
            return CmdExecResult::INTERRUPTED;
        }

        _currCmd = std::nullopt;
    }

    return result;
}

/**
 * @brief parse one command from the command queue.
 *
 * @return std::pair<Command*, std::string> command object and the arguments for the command.
 */
std::pair<Command*, std::string>
CommandLineInterface::parseOneCommandFromQueue() {
    assert(_tempCmdStored == false);
    string buffer = _commandQueue.front();
    _commandQueue.pop();

    assert(buffer[0] != '\0' && buffer[0] != ' ');

    string cmd;

    size_t n = myStrGetTok(buffer, cmd);

    if (auto pos = cmd.find_first_of('='); pos != string::npos && pos != 0) {
        string var_key = cmd.substr(0, pos);
        string var_val = cmd.substr(pos + 1);
        if (var_val.empty()) {
            logger.error("variable `{}` is not assigned a value!!", var_key);
            return {nullptr, ""};
        }
        _variables.insert_or_assign(var_key, var_val);
        return {nullptr, ""};
    }

    Command* command = getCommand(cmd);

    if (!command) {
        logger.error("Illegal command!! ({})", cmd);
        return {nullptr, ""};
    }

    string option;
    if (n != string::npos) {
        option = buffer.substr(n);
    }

    return {command, option};
}

/**
 * @brief if `str` contains the some dollar sign '$', try to convert it into variable unless it is preceded by '\'.
 *
 * @param str the string to be converted
 * @return string a string with all variables substituted with their value.
 */
string CommandLineInterface::replaceVariableKeysWithValues(string const& str) const {
    static std::regex const var_without_braces(R"(\$[\w]+)");  // if no curly braces, the variable name is until some illegal characters for a name appears

    static std::regex const var_with_braces(R"(\$\{\S+\})");  // if curly braces are used, the text inside the curly braces is the variable name
                                                              // \S means non-whitespace character

    // e.g., suppose foo_bar=apple, foo=banana
    //       "$foo_bar"     --> "apple"
    //       "$foo.bar"     --> "banana.bar"
    //       "${foo}_bar"   --> "banana_bar"
    //       "foo_$bar"     --> "foo_"
    //       "${foo}${bar}" --> "banana"

    std::vector<std::tuple<size_t, size_t, string>> to_replace;

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
                    logger.warning("Warning: variable name `{}` is illegal!!", var_key);
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

    return result;
}

/**
 * @brief return a command if and only if `cmd` is a prefix of the input string.
 *
 * @param cmd
 * @return Command*
 */
Command* CommandLineInterface::getCommand(string const& cmd) const {
    Command* e = nullptr;

    std::string copy = cmd;

    for (unsigned i = 0; i < copy.size(); ++i) {
        string check = copy.substr(0, i + 1);
        if (_cmdMap.find(check) != _cmdMap.end())
            e = _cmdMap.at(check).get();
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

/**
 * @brief return if `check` is a prefix of `_optCmd`. This command performs case-insensitive checks.
 *
 * @param check
 * @return true
 * @return false
 */
bool Command::checkOptCmd(const string& check) const {
    if (check.size() > _optCmd.size()) return false;
    for (unsigned i = 0, n = _optCmd.size(); i < n; ++i) {
        if (!check[i]) return true;
        if (tolower(_optCmd[i]) != tolower(check[i])) return false;
    }
    return true;
}
