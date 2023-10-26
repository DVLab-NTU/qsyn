/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define basic functionalities and helpers for CLI ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./cli.hpp"

#include <spdlog/spdlog.h>

#include <ranges>
#include <regex>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include "util/dvlab_string.hpp"

namespace dvlab {
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
    if (_cli_level >= dofile_stack_limit) {
        spdlog::error("dofile stack overflow ({})!!", dofile_stack_limit);
        return false;
    }

    _dofile_stack.push(std::ifstream(filepath));
    _cli_level++;

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
    _cli_level--;
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
    auto name        = cmd.get_name();
    auto n_req_chars = _identifiers.shortest_unique_prefix(cmd.get_name()).size();
    if (!cmd.initialize(n_req_chars)) {
        spdlog::error("Failed to initialize command `{}`!!", name);
        return false;
    }

    if (_commands.contains(name)) {
        spdlog::error("Command name `{}` conflicts with existing commands or aliases!!", name);
        return false;
    }

    if (_aliases.contains(name)) {
        spdlog::warn("Command name `{}` is shadowed by an alias with the same name...", name);
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
    if (_aliases.contains(alias)) {
        spdlog::warn("Overwriting the definition of alias `{}`...", alias);
    }
    if (_commands.contains(alias)) {
        spdlog::warn("Alias `{}` will shadow a command with the same name...", alias);
    }

    if (!_aliases.contains(alias)) {
        _identifiers.insert(alias);
    }
    _aliases.insert_or_assign(alias, replace_str);

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
    _aliases.erase(alias);

    for (auto& [name, c] : _commands) {
        if (auto n_req = _identifiers.shortest_unique_prefix(name).size(); n_req != c->get_num_required_chars()) {
            c->set_num_required_chars(n_req);
        }
    }

    return true;
}

bool dvlab::CommandLineInterface::add_variable(std::string const& key, std::string const& value) {
    if (_variables.contains(key)) {
        spdlog::error("Variable `{}` is already defined!!", key);
        return false;
    }
    _variables.insert_or_assign(key, value);
    return true;
}

bool dvlab::CommandLineInterface::remove_variable(std::string const& key) {
    if (!_variables.erase(key)) {
        spdlog::error("Variable `{}` is not defined!!", key);
        return false;
    }
    return true;
}

bool dvlab::CommandLineInterface::add_variables_from_dofiles(std::string const& filepath, std::span<std::string> arguments) {
    // parse the string
    // "//!ARGS <ARG1> <ARG2> ... <ARGn>"
    // and check if for all k = 1 to n,
    // _variables[to_string(k)] is mapped to a valid value

    // To enable keyword arguments, also map the names <ARGk>
    // to _variables[to_string(k)]

    std::ifstream dofile(filepath);

    if (!dofile.is_open()) {
        spdlog::error("cannot open file \"{}\"!!", filepath);
        return false;
    }

    if (dofile.peek() == std::ifstream::traits_type::eof()) {
        spdlog::error("file \"{}\" is empty!!", filepath);
        return false;
    }
    std::string line{""};
    while (line == "") {  // skip empty lines
        std::getline(dofile, line);
    };

    dofile.close();

    auto tokens = dvlab::str::views::tokenize(line, ' ') |
                  std::views::transform([](std::string_view sv) { return std::string{sv}; }) |  // regex does not support string_view
                  tl::to<std::vector<std::string>>();

    if (tokens.empty()) return true;

    if (tokens[0] == "//!ARGS") {
        tokens.erase(std::begin(tokens));
        static std::regex const valid_variable_name(R"([a-zA-Z_][\w]*)");

        std::vector<std::string> keys;
        for (auto const& token : tokens) {
            // regex does not support string_view
            if (!regex_match(token, valid_variable_name)) {
                spdlog::error("invalid argument name \"{}\" in \"//!ARGS\" directive", token);
                return false;
            }
            keys.emplace_back(token);
        }

        if (arguments.size() != keys.size()) {
            spdlog::error("wrong number of arguments provided, expected {} but got {}!!", keys.size(), arguments.size());
            spdlog::error("Usage: ... {} <{}>", filepath, fmt::join(keys, "> <"));
            return false;
        }

        for (size_t i = 0; i < keys.size(); ++i) {
            _variables.insert_or_assign(keys[i], arguments[i]);
        }
    }

    for (size_t i = 0; i < arguments.size(); ++i) {
        _variables.insert_or_assign(std::to_string(i + 1), arguments[i]);
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
        fmt::println("");
        _clear_read_buffer_and_print_prompt();
    } else if (_command_thread.has_value()) {
        // there is an executing command
        _command_thread->request_stop();
    } else {
        spdlog::critical("Failed to handle the SIGINT signal. Exiting...");
        exit(signum);
    }
}

std::optional<std::string> CommandLineInterface::_dequote(std::string_view str) const {
    std::string result;
    using parse_state = CommandLineInterface::parse_state;
    parse_state state = parse_state::normal;
    for (auto&& [i, ch] : str | tl::views::enumerate) {
        switch (state) {
            case parse_state::normal: {
                if (ch == '\'' && !_is_escaped(str, i)) {
                    state = parse_state::single_quote;
                    continue;
                } else if (ch == '\"' && !_is_escaped(str, i)) {
                    state = parse_state::double_quote;
                    continue;
                }
                break;
            }
            case parse_state::single_quote: {
                if (ch == '\'') {
                    state = parse_state::normal;
                    continue;
                }
                break;
            }
            case parse_state::double_quote: {
                if (ch == '\"' && !_is_escaped(str, i)) {
                    state = parse_state::normal;
                    continue;
                }
                break;
            }
        }

        if (_should_be_escaped(ch, state)) result += '\\';
        result += ch;
    }

    return state == parse_state::normal ? std::make_optional(result) : std::nullopt;
}

/**
 * @brief check if the character `ch` should be escaped in the given state.
 *
 * @param ch
 * @param state
 * @return true
 * @return false
 */
bool CommandLineInterface::_should_be_escaped(char ch, dvlab::CommandLineInterface::parse_state state) const {
    using parse_state = dvlab::CommandLineInterface::parse_state;
    switch (state) {
        case parse_state::normal:
            return false;
        case parse_state::single_quote:
            return dvlab::CommandLineInterface::special_chars.find(ch) != std::string::npos;
        case parse_state::double_quote:
            return dvlab::CommandLineInterface::special_chars.find(ch) != std::string::npos && dvlab::CommandLineInterface::double_quote_special_chars.find(ch) == std::string::npos;
    }
    return false;
}
/**
 * @brief check if the character at `str`[`pos`] is escaped.
 *
 * @param str
 * @param pos
 * @return true
 * @return false
 */
bool CommandLineInterface::_is_escaped(std::string_view str, size_t pos) const {
    // the first character cannot be escaped
    if (pos == 0) return false;
    // checks how many `\\` precedes the character at `pos`
    auto n = str.find_last_not_of('\\', pos - 1);

    // if all the characters preceding `str`[`pos`] are `\\`
    if (n == std::string::npos) n = 0;

    // if the number of `\\` preceding the character at `pos` is odd, it is escaped
    // e.g., the `'` in `\'` is escaped,
    //                  `\\'` is not escaped,
    //                  `\\\'` is escaped, etc.
    return (pos - n) % 2 == 0;
}
std::string CommandLineInterface::get_first_token(std::string_view str) const {
    return std::string{str.substr(0, _get_first_token_pos(str))};
}

std::string CommandLineInterface::get_last_token(std::string_view str) const {
    return std::string{str.substr(_get_last_token_pos(str))};
}

size_t CommandLineInterface::_get_first_token_pos(std::string_view str, char token) const {
    auto first_space_pos = str.find_first_of(token);
    while (first_space_pos != std::string::npos && _is_escaped(str, first_space_pos)) {
        first_space_pos = str.find_first_of(token, first_space_pos + 1);
    }
    if (first_space_pos == std::string::npos) return str.size();
    return first_space_pos;
}

size_t dvlab::CommandLineInterface::_get_last_token_pos(std::string_view str, char token) const {
    if (std::ranges::all_of(str, [token](char ch) { return ch == token; })) return std::string::npos;
    size_t pos = str.find_last_of(token);
    while (pos != std::string::npos && _is_escaped(str, pos)) {
        pos = str.find_last_of(token, pos - 1);
    }
    if (pos == std::string::npos) return 0;
    return pos + 1;
}

}  // namespace dvlab
