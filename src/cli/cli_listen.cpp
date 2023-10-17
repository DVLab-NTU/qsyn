/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Read the command from the standard input or dofile ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <spdlog/spdlog.h>
#include <termios.h>

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
 * @brief listen to input from istr and store the input in _readBuf
 *
 * @param istr
 * @param config
 * @return CmdExecResult
 */
std::pair<CmdExecResult, std::string> dvlab::CommandLineInterface::listen_to_input(std::istream& istr, std::string const& prompt, ListenConfig const& config) {
    using namespace key_code;

    /**
     * @brief this is a local RAII struct that restores the CLI settings
     *        when it goes out of scope
     *
     */
    struct settings_restorer {
        settings_restorer(CommandLineInterface* this_cli, std::string_view prompt)
            : stored_settings{set_keypress()}, stored_prompt{this_cli->_command_prompt}, p_cli{this_cli} {
            p_cli->_command_prompt       = prompt;
            p_cli->_listening_for_inputs = true;
        }
        ~settings_restorer() {
            reset_keypress(stored_settings);
            p_cli->_command_prompt       = stored_prompt;
            p_cli->_listening_for_inputs = false;
            if (p_cli->_temp_command_stored) {
                p_cli->_history.pop_back();
                p_cli->_temp_command_stored = false;
            }
        }
        settings_restorer(settings_restorer const&)            = delete;
        settings_restorer(settings_restorer&&)                 = delete;
        settings_restorer& operator=(settings_restorer const&) = delete;
        settings_restorer& operator=(settings_restorer&&)      = delete;

        termios stored_settings;
        std::string stored_prompt;
        CommandLineInterface* p_cli;
    };

    settings_restorer restorer{this, prompt};

    parse_state state = parse_state::normal;

    _reset_read_buffer();
    _print_prompt();

    while (true) {
        int keycode = get_char(istr);

        if (istr.eof()) {
            return {CmdExecResult::done, dvlab::str::trim_spaces(dvlab::str::trim_comments(_read_buffer))};
        }

        if (keycode == input_end_key) {
            return {CmdExecResult::quit, dvlab::str::trim_spaces(dvlab::str::trim_comments(_read_buffer))};
        }

        switch (keycode) {
            case newline_key: {
                if (_dequote(_read_buffer).has_value()) {
                    return {CmdExecResult::done, dvlab::str::trim_spaces(dvlab::str::trim_comments(_read_buffer))};
                } else {
                    fmt::print("\n{0:<{1}}", "...", _command_prompt.size());
                    break;
                }
            }
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
                fmt::println("");
                _reset_read_buffer();
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

/**
 * @brief move cursor position to `pos`
 *
 * @param pos
 * @return true
 * @return false
 */
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

/**
 * @brief delete a character at the cursor position
 *
 * @return true
 * @return false
 */
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

/**
 * @brief insert a character at the cursor position
 *
 * @param ch
 */
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

/**
 * @brief delete the current line on the screen and reset read buffer
 *
 */
void dvlab::CommandLineInterface::_delete_line() {
    _move_cursor_to(_read_buffer.size());
    fmt::print("{}", std::string(_cursor_position, '\b'));
    fmt::print("{}", std::string(_cursor_position, ' '));
    fmt::print("{}", std::string(_cursor_position, '\b'));
    _read_buffer.clear();
}

/**
 * @brief Reprint the current command to a newline. Cursor should be restored to the original location.
 *
 */
void dvlab::CommandLineInterface::_reprint_command() {
    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.
    size_t idx       = _cursor_position;
    _cursor_position = _read_buffer.size();  // before moving cursor, reflect the change in actual cursor location
    fmt::println("");
    _print_prompt();
    fmt::print("{}", _read_buffer);
    _move_cursor_to(idx);  // move the cursor back to where it should be
}

/**
 * @brief retrieve the command in history at index and display it on the screen
 *
 * @param index
 */
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
bool dvlab::CommandLineInterface::_add_to_history(std::string_view input) {
    if (input.size()) {
        _history.emplace_back(input);
    }

    _history_idx = int(_history.size());

    return input.size() > 0;
}

/**
 * @brief replace the read buffer with the latest history
 *
 */
void dvlab::CommandLineInterface::_replace_read_buffer_with_history() {
    _delete_line();
    _read_buffer = _history[_history_idx];
    fmt::print("{}", _read_buffer);
    _cursor_position = _history[_history_idx].size();
}

/**
 * @brief reset the read buffer
 *
 */
void dvlab::CommandLineInterface::_reset_read_buffer() {
    _read_buffer.clear();
    _cursor_position = 0;
    _tab_press_count = 0;
}

}  // namespace dvlab
