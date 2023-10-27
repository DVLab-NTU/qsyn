/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define class dvlab::CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <filesystem>
#include <fstream>
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

inline void clear_terminal() {
#ifdef _WIN32
    int const result = system("cls");
#else
    int const result = system("clear");
#endif
    if (result != 0) {
        fmt::println(stderr, "Error clearing the terminal!!");
    }
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
    Command(std::string const& name, ParserDefinition defn, OnParseSuccess on)
        : _parser{name, {.exit_on_failure = false}}, _parser_definition{std::move(defn)}, _on_parse_success{std::move(on)} {}
    Command(std::string const& name)
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

    using CmdMap = std::unordered_map<std::string, std::unique_ptr<dvlab::Command>>;

public:
    /**
     * @brief Construct a new dvlab::Command Line Interface object
     *
     * @param prompt the prompt of the CLI
     */
    CommandLineInterface(std::string const& prompt, size_t level = 0) : _command_prompt{prompt}, _cli_level{level} {
        _read_buffer.reserve(read_buf_size);
    }

    bool open_dofile(std::string const& filepath);
    void close_dofile();

    bool add_command(dvlab::Command cmd);
    bool add_alias(std::string const& alias, std::string const& replace_str);
    bool remove_alias(std::string const& alias);
    bool add_variable(std::string const& key, std::string const& value);
    bool remove_variable(std::string const& key);
    dvlab::Command* get_command(std::string const& cmd) const;
    std::optional<std::string> get_alias_replacement_string(std::string const& alias_prefix) const;

    CmdExecResult start_interactive();
    CmdExecResult execute_one_line();

    bool add_variables_from_dofiles(std::string const& filepath, std::span<std::string> arguments);

    void sigint_handler(int signum);
    bool stop_requested() const { return _command_thread.has_value() && _command_thread->get_stop_token().stop_requested(); }

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

    std::pair<CmdExecResult, std::string> listen_to_input(std::istream& istr, std::string const& prompt, ListenConfig const& config = {true, true});

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

    CmdExecResult _execute_one_line_internal(std::istream&);
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
    TabActionResult _match_identifiers(std::string const& str);
    TabActionResult _match_variables(std::string const& str);
    TabActionResult _match_files(std::string const& str);

    // helper functions
    std::vector<std::string> _get_file_matches(std::filesystem::path const& filepath) const;
    bool _autocomplete(std::string prefix_copy, std::vector<std::string> const& strs, parse_state state);
    void _print_as_table(std::vector<std::string> words) const;

    // Helper functions
    size_t _get_first_token_pos(std::string_view str, char token = ' ') const;
    size_t _get_last_token_pos(std::string_view str, char token = ' ') const;

    bool _move_cursor_to(size_t pos);
    bool _delete_char();
    void _insert_char(char);
    void _delete_line();
    void _replace_at_cursor(std::string_view old_str, std::string_view new_str);
    void _reprint_command();
    void _retrieve_history(size_t index);
    bool _add_to_history(std::string_view input);
    void _replace_read_buffer_with_history();

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
    dvlab::utils::Trie _identifiers;
    CmdMap _commands;
    std::unordered_map<std::string, std::string> _aliases;
    std::unordered_map<std::string, std::string> _variables;  // stores the variables key-value pairs, e.g., $1, $INPUT_FILE, etc...

    std::stack<std::ifstream> _dofile_stack;
    size_t _cli_level = 0;

    std::optional<jthread::jthread> _command_thread = std::nullopt;  // the current (ongoing) command
};

bool add_cli_common_cmds(dvlab::CommandLineInterface& cli);

}  // namespace dvlab
