/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define class CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <queue>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "./cli_char_def.hpp"
#include "argparse/argparse.hpp"
#include "jthread/jthread.hpp"
#include "util/logger.hpp"

class CommandLineInterface;

//----------------------------------------------------------------------
//    External declaration
//----------------------------------------------------------------------
extern CommandLineInterface CLI;
extern dvlab::Logger LOGGER;

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
    int result = system("cls");
#else
    int result = system("clear");
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
    using ParserDefinition = std::function<void(argparse::ArgumentParser&)>;
    using Precondition = std::function<bool()>;
    using OnParseSuccess = std::function<CmdExecResult(argparse::ArgumentParser const&)>;

public:
    Command(std::string const& name, ParserDefinition defn, OnParseSuccess on)
        : _parser{name, {.exitOnFailure = false}}, _parser_definition{defn}, _on_parse_success{on} {}
    Command(std::string const& name)
        : Command(name, nullptr, nullptr) {}

    bool initialize(size_t n_req_chars);
    CmdExecResult execute(std::string const& option);
    std::string const& get_name() const { return _parser.get_name(); }
    size_t get_num_required_chars() const { return _parser.get_num_required_chars(); }
    void set_num_required_chars(size_t n_req_chars) { _parser.num_required_chars(n_req_chars); }
    void print_usage() const { _parser.print_usage(); }
    void print_summary() const { _parser.print_summary(); }
    void print_help() const { _parser.print_help(); }

    void add_subcommand(Command const& cmd);

private:
    ParserDefinition _parser_definition;  // define the parser's arguments and traits
    OnParseSuccess _on_parse_success;     // define the action to take on parse success
    argparse::ArgumentParser _parser;

    void _print_missing_parser_definition_error_msg() const;
    void _print_missing_on_parse_success_error_msg() const;
};

//----------------------------------------------------------------------
//    Base class : CmdParser
//----------------------------------------------------------------------

class CommandLineInterface {
    static constexpr size_t read_buf_size = 65536;
    static constexpr size_t page_offset = 10;

    using CmdMap = std::unordered_map<std::string, std::unique_ptr<Command>>;
    using CmdRegPair = std::pair<std::string, std::unique_ptr<Command>>;

public:
    /**
     * @brief Construct a new Command Line Interface object
     *
     * @param prompt the prompt of the CLI
     */
    CommandLineInterface(std::string const& prompt) : _command_prompt{prompt} {
        _read_buffer.reserve(read_buf_size);
    }

    bool open_dofile(std::string const& filepath);
    void close_dofile();

    bool add_command(Command cmd);
    bool add_alias(std::string const& alias, std::string const& replace_str);
    bool remove_alias(std::string const& alias);
    Command* get_command(std::string const& cmd) const;

    CmdExecResult execute_one_line();

    bool add_variables_from_dofiles(std::string const& filepath, std::span<std::string> arguments);

    void sigint_handler(int signum);
    bool stop_requested() const { return _command_thread.has_value() && _command_thread->get_stop_token().stop_requested(); }

    // printing functions
    void list_all_commands() const;
    void print_history() const;
    void print_history(size_t n_print) const;

    struct ListenConfig {
        bool allow_browse_history = true;
        bool allow_tab_completion = true;
    };

    CmdExecResult listen_to_input(std::istream& istr, std::string const& prompt, ListenConfig const& config = {true, true});
    std::string const& get_read_buffer() const { return _read_buffer; }

    static std::string const special_chars;  // The characters that are identified as special characters when parsing
private:
    // Private member functions
    void _reset_buffer();
    void _print_prompt() const;

    CmdExecResult _read_one_line(std::istream&);
    std::pair<Command*, std::string> _parse_one_command_from_queue();

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
    bool _autocomplete(std::string prefix_copy, std::vector<std::string> const& strs, bool in_quotes);
    void _print_as_table(std::vector<std::string> words) const;

    // Helper functions
    bool _move_cursor_to(size_t pos);
    bool _delete_char();
    void _insert_char(char);
    void _delete_line();
    void _reprint_command();
    void _retrieve_history(size_t index);
    bool _add_input_to_history();
    void _replace_read_buffer_with_history();

    std::string _replace_variable_keys_with_values(std::string const& str) const;

    inline bool _is_special_char(char ch) const { return special_chars.find_first_of(ch) != std::string::npos; }

    // Data members
    std::string _command_prompt;
    std::string _read_buffer;
    size_t _cursor_position = 0;

    std::vector<std::string> _history;
    size_t _history_idx = 0;
    size_t _tab_press_count = 0;
    bool _listening_for_inputs = false;
    bool _temp_command_stored = false;  // When up/pgUp is pressed, current line
                                        // will be stored in _history and
                                        // _tempCmdStored will be true.
                                        // Reset to false when new command added
    dvlab::utils::Trie _identifiers;
    CmdMap _commands;
    std::unordered_map<std::string, std::string> _aliases;
    std::unordered_map<std::string, std::string> _variables;  // stores the variables key-value pairs, e.g., $1, $INPUT_FILE, etc...

    std::stack<std::ifstream> _dofile_stack;
    std::queue<std::string> _command_queue;

    std::optional<jthread::jthread> _command_thread = std::nullopt;  // the current (ongoing) command
};
