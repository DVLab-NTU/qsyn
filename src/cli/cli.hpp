/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define class dvlab::CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "./cli_char_def.hpp"
#include "argparse/arg_def.hpp"
#include "argparse/argparse.hpp"
#include "jthread/jthread.hpp"

namespace dvlab {

namespace detail {
struct heterogeneous_string_hash {
    using is_transparent = void;

    [[nodiscard]] size_t operator()(std::string_view str) const noexcept { return std::hash<std::string_view>{}(str); }
    [[nodiscard]] size_t operator()(std::string const& str) const noexcept { return std::hash<std::string>{}(str); }
    [[nodiscard]] size_t operator()(char const* const str) const noexcept { return std::hash<std::string_view>{}(str); }
};
}  // namespace detail

class CommandLineInterface;

//----------------------------------------------------------------------
//    External declaration
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//    command execution status
//----------------------------------------------------------------------
enum class CmdExecResult {
    done,
    error,
    quit,
    no_op,
    interrupted,
};
namespace detail {

inline void beep() {
    fmt::print("{}", (char)key_code::beep_char);
}

}  // namespace detail
/**
 * @brief class specification for commands that uses
 *        argparse::ArgumentParser to parse and generate help messages.
 *
 */
class Command {
    using ParserDefinition = std::function<void(dvlab::argparse::ArgumentParser&)>;
    using Precondition     = std::function<bool()>;
    using OnParseSuccess   = std::function<CmdExecResult(dvlab::argparse::ArgumentParser const&)>;

public:
    Command(std::string_view name, ParserDefinition defn, OnParseSuccess on)
        : _parser{name, {.exit_on_failure = false}}, _parser_definition{std::move(defn)}, _on_parse_success{std::move(on)} {}
    Command(std::string_view name)
        : dvlab::Command(name, nullptr, nullptr) {}

    bool initialize(size_t n_req_chars);
    CmdExecResult execute(std::vector<argparse::Token> options);
    std::string const& get_name() const { return _parser.get_name(); }
    size_t get_num_required_chars() const { return _parser.get_num_required_chars(); }
    void set_num_required_chars(size_t n_req_chars) { _parser.num_required_chars(n_req_chars); }
    void print_usage() const { _parser.print_usage(); }
    void print_summary() const { _parser.print_summary(); }
    void print_help() const { _parser.print_help(); }

    void add_subcommand(dvlab::Command const& cmd);
    void add_subcommands(std::span<dvlab::Command const> cmds);

private:
    dvlab::argparse::ArgumentParser _parser;
    ParserDefinition _parser_definition;  // define the parser's arguments and traits
    OnParseSuccess _on_parse_success;     // define the action to take on parse success

    void _print_missing_parser_definition_error_msg() const;
    void _print_missing_on_parse_success_error_msg() const;
};

//----------------------------------------------------------------------
//    Base class : CmdParser
//----------------------------------------------------------------------

class CommandLineInterface {
    static constexpr size_t read_buf_size = 65536;
    static constexpr size_t page_offset   = 10;

    using CmdMap = std::unordered_map<std::string, std::unique_ptr<dvlab::Command>, detail::heterogeneous_string_hash, std::equal_to<>>;

public:
    /**
     * @brief Construct a new dvlab::Command Line Interface object
     *
     * @param prompt the prompt of the CLI
     */
    CommandLineInterface(std::string_view prompt, size_t level = 0) : _command_prompt{prompt}, _cli_level{level} {
        _read_buffer.reserve(read_buf_size);
    }

    CmdExecResult execute_one_line(std::istream& istr, bool echo);
    CmdExecResult source_dofile(std::filesystem::path const& filepath, std::span<std::string const> arguments = {}, bool echo = true);

    bool add_command(dvlab::Command cmd);
    bool add_alias(std::string_view alias, std::string_view replace_str);
    bool remove_alias(std::string_view alias);
    bool add_variable(std::string_view key, std::string_view value);
    bool remove_variable(std::string_view key);
    dvlab::Command* get_command(std::string_view cmd) const;
    std::optional<std::string> get_alias_replacement_string(std::string_view alias_prefix) const;

    CmdExecResult start_interactive();

    bool add_variables_from_dofiles(std::filesystem::path const& filepath, std::span<std::string const> arguments);

    void sigint_handler(int signum);
    bool stop_requested() const { return _command_threads.size() && _command_threads.top().get_stop_token().stop_requested(); }

    // printing functions
    void list_all_commands() const;
    void list_all_aliases() const;
    void list_all_variables() const;
    void print_history() const;
    void print_history(size_t n_print) const;

    struct ListenConfig {
        bool allow_browse_history = true;
        bool allow_tab_completion = true;
    };

    std::pair<CmdExecResult, std::string> listen_to_input(std::istream& istr, std::string_view prompt, ListenConfig const& config = {true, true});

    constexpr static std::string_view double_quote_special_chars = "\\$";        // The characters that are identified as special characters when parsing inside double quotes
    constexpr static std::string_view special_chars              = "\\$\"\' ;";  // The characters that are identified as special characters when parsing

    enum class parse_state {
        normal,
        single_quote,
        double_quote,
    };

    std::string get_first_token(std::string_view str) const;
    std::string get_last_token(std::string_view str) const;

private:
    // Private member functions
    void _clear_read_buffer_and_print_prompt();

    std::pair<dvlab::Command*, std::vector<argparse::Token>> _parse_one_command(std::string_view cmd);
    std::optional<std::string> _dequote(std::string_view str) const;
    std::string _decode(std::string str) const;
    CmdExecResult _dispatch_command(dvlab::Command* cmd, std::vector<argparse::Token> options);
    bool _is_escaped(std::string_view str, size_t pos) const;
    bool _should_be_escaped(char ch, dvlab::CommandLineInterface::parse_state state) const;

    enum class TabActionResult {
        autocomplete,
        list_options,
        no_op
    };
    // tab-related features features
    void _on_tab_pressed();
    // onTabPressed subroutines
    TabActionResult _match_identifiers(std::string_view str);
    // NOTE - This function passes the string by const ref instead of string_view because it uses std::regex
    TabActionResult _match_variables(std::string const& str);
    TabActionResult _match_files(std::string_view str);

    // helper functions
    std::vector<std::string> _get_file_matches(std::filesystem::path const& filepath) const;
    bool _autocomplete(std::string prefix_copy, std::vector<std::string> const& strs, parse_state state);
    void _print_as_table(std::vector<std::string> words) const;

    // Helper functions
    size_t _get_first_token_pos(std::string_view str, char token = ' ') const;
    size_t _get_last_token_pos(std::string_view str, char token = ' ') const;

    bool _move_cursor_to(size_t pos);
    bool _delete_char();
    void _insert_char(char ch);
    void _delete_line();
    void _replace_at_cursor(std::string_view old_str, std::string_view new_str);
    void _reprint_command();
    void _retrieve_history(size_t index);
    bool _add_to_history(std::string_view input);
    void _replace_read_buffer_with_history();

    template <typename... Args>
    void _print_if_echo(fmt::format_string<Args...> fmt, Args&&... args) const {
        if (_echo)
            fmt::print(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void _println_if_echo(fmt::format_string<Args...> fmt, Args&&... args) const {
        if (_echo)
            fmt::println(fmt, std::forward<Args>(args)...);
    }

    void _flush_if_echo() const {
        if (_echo)
            fflush(stdout);
    }
    // NOTE - This function passes the string by const ref instead of string_view because it uses std::regex
    std::string _replace_variable_keys_with_values(std::string const& str) const;

    inline bool _is_special_char(char ch) const { return special_chars.find_first_of(ch) != std::string::npos; }

    // Data members
    std::string _command_prompt;
    std::string _read_buffer;
    size_t _cursor_position = 0;

    std::vector<std::string> _history;
    size_t _history_idx        = 0;
    size_t _tab_press_count    = 0;
    bool _listening_for_inputs = false;
    bool _temp_command_stored  = false;  // When up/pgUp is pressed, current line
                                         // will be stored in _history and
                                         // _tempCmdStored will be true.
                                         // Reset to false when new command added
    bool _echo = true;
    dvlab::utils::Trie _identifiers;
    CmdMap _commands;
    std::unordered_map<std::string, std::string, detail::heterogeneous_string_hash, std::equal_to<>> _aliases;
    std::unordered_map<std::string, std::string, detail::heterogeneous_string_hash, std::equal_to<>> _variables;  // stores the variables key-value pairs, e.g., $1, $INPUT_FILE, etc...

    size_t _cli_level = 0;

    std::stack<jthread::jthread> _command_threads;
};

bool add_cli_common_cmds(dvlab::CommandLineInterface& cli);

}  // namespace dvlab
