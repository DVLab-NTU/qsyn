/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define command parsing and execution functions for CLI ]
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

namespace dvlab {

std::string const dvlab::CommandLineInterface::special_chars = "\"\' ;$";

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
bool dvlab::CommandLineInterface::open_dofile(std::string const& filepath) {
    constexpr size_t dofile_stack_limit = 256;
    if (this->stop_requested()) {
        return false;
    }
    if (_dofile_stack.size() >= dofile_stack_limit) {
        LOGGER.error("dofile stack overflow ({})!!", dofile_stack_limit);
        return false;
    }

    _dofile_stack.push(std::move(std::ifstream(filepath)));

    if (!_dofile_stack.top().is_open()) {
        close_dofile();
        return false;
    }
    return true;
}

/**
 * @brief close the top dofile in the dofile stack.
 *
 */
void dvlab::CommandLineInterface::close_dofile() {
    assert(_dofile_stack.size());
    _dofile_stack.pop();
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
bool dvlab::CommandLineInterface::add_command(dvlab::Command cmd) {
    // Make sure cmd hasn't been registered and won't cause ambiguity
    auto name = cmd.get_name();
    auto n_req_chars = _identifiers.shortest_unique_prefix(cmd.get_name()).size();
    if (!cmd.initialize(n_req_chars)) {
        LOGGER.error("Failed to initialize command `{}`!!", name);
        return false;
    }

    if (_identifiers.contains(name)) {
        LOGGER.error("dvlab::Command name `{}` conflicts with existing commands or aliases!!", name);
        return false;
    }
    _identifiers.insert(name);
    _commands.emplace(name, std::make_unique<dvlab::Command>(std::move(cmd)));

    for (auto& [name, c] : _commands) {
        if (auto n_req = _identifiers.shortest_unique_prefix(name).size(); n_req != c->get_num_required_chars()) {
            c->set_num_required_chars(n_req);
        }
    }
    return true;
}

bool dvlab::CommandLineInterface::add_alias(std::string const& alias, std::string const& replace_str) {
    if (_identifiers.contains(alias)) {
        LOGGER.error("Alias `{}` conflicts with existing commands or aliases!!", alias);
        return false;
    }

    string first_token;
    size_t n = dvlab::str::str_get_token(replace_str, first_token);
    if (auto freq = _identifiers.frequency(first_token); freq != 1) {
        if (freq > 1) {
            LOGGER.error("Ambiguous command or alias `{}`!!", first_token);
        } else {
            LOGGER.error("Unknown command or alias `{}`!!", first_token);
        }
        return false;
    }

    _identifiers.insert(alias);
    _aliases.emplace(alias, replace_str);

    for (auto& [name, c] : _commands) {
        if (auto n_req = _identifiers.shortest_unique_prefix(name).size(); n_req != c->get_num_required_chars()) {
            c->set_num_required_chars(n_req);
        }
    }

    return true;
}

bool dvlab::CommandLineInterface::remove_alias(std::string const& alias) {
    if (!_identifiers.erase(alias)) {
        return false;
    }
    auto n = _aliases.erase(alias);

    for (auto& [name, c] : _commands) {
        if (auto n_req = _identifiers.shortest_unique_prefix(name).size(); n_req != c->get_num_required_chars()) {
            c->set_num_required_chars(n_req);
        }
    }

    return true;
}

/**
 * @brief handle the SIGINT signal. Wrap the handler in a static function so that it can be passed to the signal function.
 *
 * @param signum
 */
void dvlab::CommandLineInterface::sigint_handler(int signum) {
    if (_listening_for_inputs) {
        _reset_buffer();
        fmt::print("\n");
        _print_prompt();
    } else if (_command_thread.has_value()) {
        // there is an executing command
        _command_thread->request_stop();
    } else {
        LOGGER.fatal("Failed to handle the SIGINT signal. Exiting...");
        exit(signum);
    }
}

/**
 * @brief execute one line of commands.
 *
 * @return CmdExecResult
 */
CmdExecResult
dvlab::CommandLineInterface::execute_one_line() {
    while (_dofile_stack.size() && _dofile_stack.top().eof()) close_dofile();
    if (auto result = (_dofile_stack.size() ? _read_one_line(_dofile_stack.top()) : _read_one_line(std::cin)); result != CmdExecResult::done) {
        if (_dofile_stack.empty() && std::cin.eof()) return CmdExecResult::quit;
        return result;
    }

    // execute the command

    std::atomic<CmdExecResult> result;

    while (_command_queue.size()) {
        auto [cmd, option] = _parse_one_command_from_queue();

        if (cmd == nullptr) continue;

        _command_thread = jthread::jthread(
            [this, &cmd = cmd, &option = option, &result]() {
                result = cmd->execute(option);
            });

        assert(_command_thread.has_value());

        _command_thread->join();

        if (this->stop_requested()) {
            LOGGER.warning("dvlab::Command interrupted");
            while (_command_queue.size()) _command_queue.pop();
            return CmdExecResult::interrupted;
        }

        _command_thread = std::nullopt;
    }

    return result;
}

/**
 * @brief parse one command from the command queue.
 *
 * @return std::pair<dvlab::Command*, std::string> command object and the arguments for the command.
 */
std::pair<dvlab::Command*, std::string>
dvlab::CommandLineInterface::_parse_one_command_from_queue() {
    assert(_temp_command_stored == false);
    string buffer = _command_queue.front();
    _command_queue.pop();

    assert(buffer[0] != '\0' && buffer[0] != ' ');

    string first_token;
    string option;

    size_t n = dvlab::str::str_get_token(buffer, first_token);
    if (n != string::npos) {
        option = buffer.substr(n);
    }

    if (auto pos = first_token.find_first_of('='); pos != string::npos && pos != 0) {
        string var_key = first_token.substr(0, pos);
        string var_val = first_token.substr(pos + 1);
        if (var_val.empty()) {
            LOGGER.error("variable `{}` is not assigned a value!!", var_key);
            return {nullptr, ""};
        }
        _variables.insert_or_assign(var_key, var_val);
        return {nullptr, ""};
    }

    if (auto freq = _identifiers.frequency(first_token); freq != 1) {
        if (freq > 1) {
            LOGGER.error("Ambiguous command or alias `{}`!!", first_token);
        } else {
            LOGGER.error("Unknown command or alias `{}`!!", first_token);
        }
        return {nullptr, ""};
    }

    auto identifier = _identifiers.find_with_prefix(first_token);
    if (!identifier.has_value()) {
        if (_identifiers.frequency(first_token) > 0) {
            LOGGER.error("Ambiguous command or alias `{}`!!", first_token);
        } else {
            LOGGER.error("Unknown command or alias `{}`!!", first_token);
        }
        return {nullptr, ""};
    }
    assert(_commands.contains(*identifier) || _aliases.contains(*identifier));

    if (_aliases.contains(*identifier)) {
        auto alias = _aliases.at(*identifier);
        size_t pos = dvlab::str::str_get_token(alias, first_token);
        if (pos != string::npos) {
            option = alias.substr(pos) + " " + option;
            first_token = alias.substr(0, pos);
        } else {
            first_token = alias;
        }
    }

    dvlab::Command* command = get_command(first_token);

    if (!command) {
        LOGGER.error("Illegal command!! ({})", first_token);
        return {nullptr, ""};
    }

    return {command, option};
}

/**
 * @brief if `str` contains the some dollar sign '$', try to convert it into variable unless it is preceded by '\'.
 *
 * @param str the string to be converted
 * @return string a string with all variables substituted with their value.
 */
string dvlab::CommandLineInterface::_replace_variable_keys_with_values(string const& str) const {
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
    // FIXME - doesn't work for nested variables and multiple variables in one line
    for (auto const& re : {var_without_braces, var_with_braces}) {
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
                    LOGGER.warning("Warning: variable name `{}` is illegal!!", var_key);
                    break;
                }
            }

            size_t pos = matches.position(i);
            if (dvlab::str::is_escaped_char(str, pos)) {
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
 * @return dvlab::Command*
 */
dvlab::Command* dvlab::CommandLineInterface::get_command(string const& cmd) const {
    dvlab::Command* e = nullptr;

    auto match = _identifiers.find_with_prefix(cmd);
    if (match.has_value()) {
        return _commands.at(*match).get();
    }

    return nullptr;
}

}  // namespace dvlab