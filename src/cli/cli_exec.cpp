/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define command parsing and execution functions for CLI ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <spdlog/spdlog.h>

#include <atomic>
#include <regex>
#include <string>
#include <tl/enumerate.hpp>

#include "cli/cli.hpp"
#include "fmt/core.h"
#include "util/util.hpp"

namespace dvlab {

//----------------------------------------------------------------------
//    Member Function for class cmdParser
//----------------------------------------------------------------------

CmdExecResult CommandLineInterface::start_interactive() {
    auto status = dvlab::CmdExecResult::done;

    while (status != dvlab::CmdExecResult::quit) {  // until "quit" or command error
        status = this->execute_one_line();
        fmt::println("");
    }

    return status;
}

/**
 * @brief execute one line of commands.
 *
 * @return CmdExecResult
 */
CmdExecResult
dvlab::CommandLineInterface::execute_one_line() {
    while (_dofile_stack.size() && _dofile_stack.top().eof()) close_dofile();

    return _dofile_stack.size()
               ? _execute_one_line_internal(_dofile_stack.top())
               : _execute_one_line_internal(std::cin);
}

/**
 * @brief
 *
 * @param istr
 * @return CmdExecResult
 */
CmdExecResult dvlab::CommandLineInterface::_execute_one_line_internal(std::istream& istr) {
    auto [listen_result, input] = this->listen_to_input(istr, _command_prompt);
    if (listen_result == CmdExecResult::quit) {
        return CmdExecResult::quit;
    }

    if (!_add_to_history(input)) {
        return CmdExecResult::no_op;
    }

    fmt::println("");

    auto stripped = _dequote(input);

    if (!stripped.has_value()) {
        spdlog::critical("Unexpected error: dequote failed");
        return CmdExecResult::no_op;
    }

    CmdExecResult exec_result = CmdExecResult::done;

    while (true) {
        size_t semicolon_pos = stripped->find_first_of(';');
        while (semicolon_pos != std::string::npos && _is_escaped(stripped.value(), semicolon_pos)) {
            semicolon_pos = stripped->find_first_of(';', semicolon_pos + 1);
        }
        if (semicolon_pos == std::string::npos) {
            semicolon_pos = stripped->size();
        }
        if (semicolon_pos > 0) {
            auto this_cmd      = stripped->substr(0, semicolon_pos);
            auto [cmd, option] = _parse_one_command(this_cmd);
            if (cmd != nullptr) {
                exec_result = _dispatch_command(cmd, option);
            }
        }
        if (semicolon_pos == stripped->size()) {
            break;
        }
        stripped = stripped->substr(semicolon_pos + 1);
    }

    return exec_result;
}

/**
 * @brief parse one command from the command queue.
 *
 * @return std::pair<dvlab::Command*, std::string> command object and the arguments for the command.
 */
std::pair<dvlab::Command*, std::vector<argparse::Token>>
dvlab::CommandLineInterface::_parse_one_command(std::string_view cmd) {
    assert(_temp_command_stored == false);

    std::string buffer{cmd};

    assert(buffer[0] != '\0' && buffer[0] != ' ');

    // get the first token. The first token should be a command or an alias
    std::string first_token;

    size_t n = dvlab::str::str_get_token(buffer, first_token);
    if (n == std::string::npos) {
        n = buffer.size();
    }

    auto identifier = _identifiers.find_with_prefix(first_token);
    if (!identifier.has_value()) {
        if (_identifiers.frequency(first_token) > 0) {
            spdlog::error("Ambiguous command or alias `{}`!!", first_token);
            // There's no need to guard the case where identifier is empty
            // because if first_token does not match any command or alias,
            // it may still be a variable
            return {nullptr, {}};
        }
    } else {
        // if the first token is an alias, replace it with the replacement string and update the first token
        if (_aliases.contains(*identifier)) {
            buffer = _aliases.at(*identifier) + buffer.substr(n);
        }
    }

    // replace all variables with their values
    buffer = _replace_variable_keys_with_values(buffer);

    // get the first token again (since the first token may be a variable)
    n = dvlab::str::str_get_token(buffer, first_token);

    dvlab::Command* command = get_command(first_token);

    if (!command) {
        spdlog::error("Unknown command!! ({})", first_token);
        return {nullptr, {}};
    }

    // tokenize the rest of the buffer
    if (n == std::string::npos) {
        n = buffer.size();
    }
    buffer = buffer.substr(n);

    for (auto& ch : buffer) {
        using namespace std::string_view_literals;
        if ("\t\v\r\n\f"sv.find(ch) != std::string::npos) {
            ch = ' ';
        }
    }

    std::vector<argparse::Token> arguments;

    while (true) {
        size_t space_pos = buffer.find_first_of(' ');
        while (space_pos != std::string::npos && _is_escaped(buffer, space_pos)) {
            space_pos = buffer.find_first_of(';', space_pos + 1);
        }
        if (space_pos == std::string::npos) {
            space_pos = buffer.size();
        }
        if (space_pos > 0) {
            auto token = buffer.substr(0, space_pos);
            arguments.emplace_back(_decode(token));
        }
        if (space_pos == buffer.size()) {
            break;
        }
        buffer = buffer.substr(space_pos + 1);
    }

    return {command, arguments};
}

CmdExecResult CommandLineInterface::_dispatch_command(dvlab::Command* cmd, std::vector<argparse::Token> options) {
    std::atomic<CmdExecResult> exec_result = CmdExecResult::done;

    _command_thread = jthread::jthread(
        [&cmd, &options, &exec_result]() {
            exec_result = cmd->execute(options);
        });

    assert(_command_thread.has_value());

    _command_thread->join();

    if (this->stop_requested()) {
        spdlog::warn("Command interrupted");
        return CmdExecResult::interrupted;
    }

    _command_thread = std::nullopt;

    return exec_result;
}

/**
 * @brief if `str` contains the some dollar sign '$', try to convert it into variable unless it is preceded by '\'.
 *
 * @param str the string to be converted
 * @return string a string with all variables substituted with their value.
 */
std::string dvlab::CommandLineInterface::_replace_variable_keys_with_values(std::string const& str) const {
    static std::regex const var_without_braces(R"(\$[\w]+)");  // if no curly braces, the variable name is until some illegal characters for a name appears

    static std::regex const var_with_braces(R"(\$\{\S+?\})");  // if curly braces are used, the text inside the curly braces is the variable name
                                                               // \S means non-whitespace character

    // e.g., suppose foo_bar=apple, foo=banana
    //       "$foo_bar"     --> "apple"
    //       "$foo.bar"     --> "banana.bar"
    //       "${foo}_bar"   --> "banana_bar"
    //       "foo_$bar"     --> "foo_"
    //       "${foo}${bar}" --> "banana"

    std::vector<std::tuple<size_t, size_t, std::string>> to_replace;
    // FIXME - doesn't work for nested variables and multiple variables in one line
    for (auto const& re : {var_without_braces, var_with_braces}) {
        std::smatch matches;
        std::sregex_token_iterator regex_end;
        std::sregex_token_iterator regex_begin(str.begin(), str.end(), re);
        for (auto regex_itr = regex_begin; regex_itr != regex_end; ++regex_itr) {
            auto var = (*regex_itr).str();
            // tell if it is a curly brace variable or not
            bool is_brace       = var[1] == '{';
            std::string var_key = is_brace ? var.substr(2, var.length() - 3) : var.substr(1);
            bool is_defined     = _variables.contains(var_key);
            std::string val     = is_defined ? _variables.at(var_key) : "";

            if (is_brace && !is_defined) {
                for (auto ch : var_key) {
                    if (isalnum(ch) || ch == '_') {
                        continue;
                    }
                    spdlog::warn("variable name `{}` is illegal!!", var_key);
                    break;
                }
            }

            size_t pos = std::distance(str.begin(), regex_itr->first);
            if (_is_escaped(str, pos)) {
                continue;
            }
            to_replace.emplace_back(pos, var.length(), val);
        }
    }

    std::ranges::sort(to_replace, [](auto const& lhs, auto const& rhs) { return std::get<0>(lhs) < std::get<0>(rhs); });

    size_t cursor      = 0;
    std::string result = "";
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
dvlab::Command* dvlab::CommandLineInterface::get_command(std::string const& cmd) const {
    auto match = _identifiers.find_all_with_prefix(cmd);
    for (auto const& identifier : match) {
        if (_commands.contains(identifier)) {
            return _commands.at(identifier).get();
        }
    }

    return nullptr;
}

std::string dvlab::CommandLineInterface::_decode(std::string str) const {
    for (size_t i = 0; i < str.size() - 1; ++i) {
        if (str[i] == '\\') {
            switch (str[i + 1]) {
                case 'a':
                    str[i] = '\a';
                    break;
                case 'b':
                    str[i] = '\b';
                    break;
                case 'c':
                    return str.substr(0, i);
                case 'e':
                    str[i] = '\e';
                    break;
                case 'f':
                    str[i] = '\f';
                    break;
                case 'n':
                    str[i] = '\n';
                    break;
                case 'r':
                    str[i] = '\r';
                    break;
                case 't':
                    str[i] = '\t';
                    break;
                case 'v':
                    str[i] = '\v';
                    break;
                case '\\':
                case '\'':
                case '\"':
                case ' ':
                case ';':
                case '$':
                default:
                    str[i] = str[i + 1];
                    break;
            }
            str.erase(i + 1, 1);
        }
    }
    return str;
}

}  // namespace dvlab
