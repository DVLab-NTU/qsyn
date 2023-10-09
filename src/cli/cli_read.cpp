/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Read the command from the standard input or dofile ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <spdlog/spdlog.h>
#include <termios.h>

#include <cassert>
#include <cstring>
#include <limits>
#include <regex>
#include <sstream>

#include "./cli.hpp"
#include "cli/cli_char_def.hpp"

//----------------------------------------------------------------------
//    Member Function for class CmdParser
//----------------------------------------------------------------------

namespace {

/**
 * @brief restores the terminal settings
 *
 * @param stored_settings
 * @return auto
 */
auto reset_keypress(termios const& stored_settings) {
    tcsetattr(0, TCSANOW, &stored_settings);
}

/**
 * @brief enables the terminal to read one char at a time, and don't echo the input to terminal
 *
 * @return termios the original terminal settings. This is used to restore the terminal settings
 */
[[nodiscard]] auto set_keypress() -> termios {
    termios stored_settings{};
    tcgetattr(0, &stored_settings);
    termios new_settings = stored_settings;
    new_settings.c_lflag &= (~ICANON);  // make sure we can read one char at a time
    new_settings.c_lflag &= (~ECHO);    // don't print input characters. We would like to handle them ourselves
    new_settings.c_cc[VTIME] = 0;       // start reading immediately
    new_settings.c_cc[VMIN]  = 1;       // ...and wait until we get one char to return
    tcsetattr(0, TCSANOW, &new_settings);

    return stored_settings;
}

auto mygetc(std::istream& istr) -> char {
    char ch = 0;
    istr.get(ch);
    return ch;
}

int get_char(std::istream& istr) {
    using namespace dvlab::key_code;
    char ch = mygetc(istr);

    assert(ch != interrupt_key);
    switch (ch) {
        // Simple keys: one code for one key press
        // -- The following should be platform-independent
        case line_begin_key:      // Ctrl-a
        case line_end_key:        // Ctrl-e
        case input_end_key:       // Ctrl-d
        case tab_key:             // tab('\t') or Ctrl-i
        case newline_key:         // enter('\n') or ctrl-m
        case clear_terminal_key:  // Clear terminal (Ctrl-l)
            return ch;

        // -- The following simple/combo keys are platform-dependent
        //    You should test to check the returned codes of these key presses
        // -- You should either modify the key_code definitions in
        //    "cliCharDef.hpp", or revise the control flow of the "case ESC" below
        case back_space_key:
            return ch;
        case back_space_char:
            return back_space_key;

        // Combo keys: multiple codes for one key press
        // -- Usually starts with ESC key, so we check the "case ESC"
        case esc_key: {
            char combo = mygetc(istr);
            // Note: ARROW_KEY_INT == MOD_KEY_INT, so we only check MOD_KEY_INT
            if (combo == char(mod_key_int)) {
                char key = mygetc(istr);
                if ((key >= char(mod_key_begin)) && (key <= char(mod_key_end))) {
                    if (mygetc(istr) == mod_key_dummy)
                        return int(key) + mod_key_flag;
                    else
                        return undefined_key;
                } else if ((key >= char(arrow_key_begin)) &&
                           (key <= char(arrow_key_end)))
                    return int(key) + arrow_key_flag;
                else
                    return undefined_key;
            } else {
                dvlab::detail::beep();
                return get_char(istr);
            }
        }
        // For the remaining printable and undefined keys
        default:
            return (isprint(ch)) ? ch : undefined_key;
    }
}

}  // namespace

namespace dvlab {

/**
 * @brief reset the read buffer
 *
 */
void dvlab::CommandLineInterface::_reset_buffer() {
    _read_buffer.clear();
    _cursor_position = 0;
    _tab_press_count = 0;
}

/**
 * @brief listen to input from istr and store the input in _readBuf
 *
 * @param istr
 * @param config
 * @return CmdExecResult
 */
CmdExecResult dvlab::CommandLineInterface::listen_to_input(std::istream& istr, std::string const& prompt, ListenConfig const& config) {
    using namespace key_code;

    auto stored_prompt = _command_prompt;  // save the original _prompt. We do this because signal handlers cannot take extra arguments

    _command_prompt       = prompt;
    _listening_for_inputs = true;

    _reset_buffer();
    _print_prompt();

    auto stored_settings = set_keypress();
    while (true) {
        int keycode = get_char(istr);

        if (istr.eof()) {
            reset_keypress(stored_settings);
            return CmdExecResult::done;
        }

        if (keycode == input_end_key) {
            reset_keypress(stored_settings);
            return CmdExecResult::quit;
        }

        switch (keycode) {
            case newline_key:
                reset_keypress(stored_settings);
                _command_prompt       = stored_prompt;
                _listening_for_inputs = false;
                return CmdExecResult::done;
            case line_begin_key:
            case home_key:
                _move_cursor_to(0);
                break;
            case line_end_key:
            case end_key:
                _move_cursor_to(_read_buffer.size());
                break;
            case back_space_key:
                if (_cursor_position == 0) {
                    detail::beep();
                } else {
                    _move_cursor_to(_cursor_position - 1);
                    _delete_char();
                }
                break;
            case delete_key:
                _delete_char();
                break;
            case clear_terminal_key:
                detail::clear_terminal();
                fmt::print("\n");
                _reset_buffer();
                _print_prompt();
                break;
            case arrow_up_key:
                if (!config.allow_browse_history || _history_idx == 0) {
                    detail::beep();
                } else {
                    _retrieve_history(_history_idx - 1);
                }
                break;
            case arrow_down_key:
                (config.allow_browse_history) ? _retrieve_history(_history_idx + 1) : detail::beep();
                break;
            case arrow_right_key:
                if (_cursor_position == _read_buffer.size()) {
                    detail::beep();
                } else {
                    _move_cursor_to(_cursor_position + 1);
                }
                break;
            case arrow_left_key:
                _move_cursor_to((int)_cursor_position - 1);
                break;
            case pg_up_key:
                (config.allow_browse_history) ? _retrieve_history(_history_idx - std::min(page_offset, _history_idx)) : detail::beep();
                break;
            case pg_down_key:
                (config.allow_browse_history) ? _retrieve_history(_history_idx + page_offset) : detail::beep();
                break;
            case tab_key: {
                if (config.allow_tab_completion) {
                    ++_tab_press_count;
                    _on_tab_pressed();
                } else {
                    detail::beep();
                }
                break;
            }
            case insert_key:  // not yet supported; fall through to UNDEFINE
            case undefined_key:
                detail::beep();
                break;
            default:  // printable character
                _insert_char(char(keycode));
                break;
        }
    }
}

CmdExecResult dvlab::CommandLineInterface::_read_one_line(std::istream& istr) {
    auto result = this->listen_to_input(istr, _command_prompt);

    if (result == CmdExecResult::quit) {
        return CmdExecResult::quit;
    }

    if (!_add_input_to_history()) {
        return CmdExecResult::no_op;
    }

    auto stripped = dvlab::str::strip_quotes(_history.back()).value_or("");

    stripped                        = _replace_variable_keys_with_values(stripped);
    std::vector<std::string> tokens = dvlab::str::split(stripped, ";");

    if (tokens.size()) {
        // concat tokens with '\;' to a single token
        for (auto itr = next(tokens.rbegin()); itr != tokens.rend(); ++itr) {
            std::string& curr_token = *itr;
            std::string& next_token = *prev(itr);

            if (curr_token.ends_with('\\') && !curr_token.ends_with("\\\\")) {
                curr_token.back() = ';';
                curr_token += next_token;
                next_token = "";
            }
        }
        erase_if(tokens, [](std::string const& token) { return token == ""; });
        std::ranges::for_each(tokens, [this](std::string& token) { _command_queue.push(dvlab::str::strip_spaces(token)); });
    }

    fmt::print("\n");
    fflush(stdout);

    return CmdExecResult::done;
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
bool dvlab::CommandLineInterface::_move_cursor_to(size_t pos) {
    if (pos > _read_buffer.size()) {  // since pos is unsigned, this should also checks if pos < 0
        detail::beep();
        return false;
    }

    // move left
    if (_cursor_position > pos) {
        fmt::print("{}", std::string(_cursor_position - pos, '\b'));
    }

    // move right
    if (_cursor_position < pos) {
        fmt::print("{}", _read_buffer.substr(_cursor_position, pos - _cursor_position));
    }
    _cursor_position = pos;
    return true;
}

bool dvlab::CommandLineInterface::_delete_char() {
    if (_cursor_position == _read_buffer.size()) {
        detail::beep();
        return false;
    }
    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.
    fmt::print("{}", _read_buffer.substr(_cursor_position + 1));  // move cursor to the end
    fmt::print(" \b");                                            // get rid of the last character

    _read_buffer.erase(_cursor_position, 1);

    size_t idx       = _cursor_position;
    _cursor_position = _read_buffer.size();  // before moving cursor, reflect the change in actual cursor location
    _move_cursor_to(idx);                    // move the cursor back to where it should be
    return true;
}

void dvlab::CommandLineInterface::_insert_char(char ch) {
    if (_read_buffer.size() >= std::numeric_limits<int>::max()) {
        detail::beep();
        return;
    }
    _read_buffer.insert(_cursor_position, 1, ch);
    fmt::print("{}", _read_buffer.substr(_cursor_position));
    size_t idx       = _cursor_position + 1;
    _cursor_position = _read_buffer.size();
    _move_cursor_to(idx);
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
void dvlab::CommandLineInterface::_delete_line() {
    _move_cursor_to(_read_buffer.size());
    fmt::print("{}", std::string(_cursor_position, '\b'));
    fmt::print("{}", std::string(_cursor_position, ' '));
    fmt::print("{}", std::string(_cursor_position, '\b'));
    _read_buffer.clear();
}

// Reprint the current command to a newline
// cursor should be restored to the original location
void dvlab::CommandLineInterface::_reprint_command() {
    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.
    size_t idx       = _cursor_position;
    _cursor_position = _read_buffer.size();  // before moving cursor, reflect the change in actual cursor location
    fmt::println("");
    _print_prompt();
    fmt::print("{}", _read_buffer);
    _move_cursor_to(idx);  // move the cursor back to where it should be
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
void dvlab::CommandLineInterface::_retrieve_history(size_t index) {
    if (index == _history_idx) return;

    if (index < _history_idx) {                 // move up
        if (_history_idx == _history.size()) {  // move away from new input
            _temp_command_stored = true;
            _history.emplace_back(_read_buffer);
        } else if (_temp_command_stored && _history_idx == _history.size() - 1)
            _history.back() = _read_buffer;  // => update it
    } else if (index > _history_idx) {       // move down
        if (_history_idx == _history.size() - _temp_command_stored ? 1 : 0) {
            detail::beep();
            return;
        }
        if (index >= _history.size() - 1)
            index = _history.size() - 1;
    }

    _history_idx = index;
    _replace_read_buffer_with_history();
}

/**
 * @brief Add the command in buffer to _history.
 *        This function trim the comment, leading/trailing whitespace of the entered comments
 *
 */
bool dvlab::CommandLineInterface::_add_input_to_history() {
    if (_temp_command_stored) {
        _history.pop_back();
        _temp_command_stored = false;
    }

    std::string cmd = dvlab::str::strip_spaces(dvlab::str::strip_comments(_read_buffer));

    if (cmd.size()) {
        _history.emplace_back(cmd);
    }

    _history_idx = int(_history.size());

    return cmd.size() > 0;
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

    std::vector<std::string> tokens = dvlab::str::split(line, " ");

    std::erase_if(tokens, [](std::string const& token) { return token == ""; });

    if (tokens.empty()) return true;

    if (tokens[0] == "//!ARGS") {
        tokens.erase(std::begin(tokens));
        static std::regex const valid_variable_name(R"([a-zA-Z_][\w]*)");

        std::vector<std::string> keys;
        for (auto const& token : tokens) {
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

// 1. Replace current line with _history[_historyIdx] on the screen
// 2. Set _readBufPtr and _readBufEnd to end of line
//
// [Note] Do not change _history.size().
//
void dvlab::CommandLineInterface::_replace_read_buffer_with_history() {
    _delete_line();
    _read_buffer = _history[_history_idx];
    fmt::print("{}", _read_buffer);
    _cursor_position = _history[_history_idx].size();
}

}  // namespace dvlab
