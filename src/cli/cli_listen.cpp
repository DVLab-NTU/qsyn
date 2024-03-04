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
#include "util/dvlab_string.hpp"
#include "util/scope_guard.hpp"
#include "util/sysdep.hpp"

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
    auto const ch = mygetc(istr);

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
            auto const combo = mygetc(istr);
            // Note: ARROW_KEY_INT == CTRL_KEY_INT, so we only check CTRL_KEY_INT
            if (combo == char(ctrl_key_int)) {
                auto const key = mygetc(istr);
                if ((key >= char(ctrl_key_begin)) && (key <= char(ctrl_key_end))) {
                    if (mygetc(istr) == ctrl_key_dummy)
                        return int(key) + ctrl_key_flag;
                    else
                        return undefined_key;
                } else if ((key >= char(arrow_key_begin)) &&
                           (key <= char(arrow_key_end)))
                    return int(key) + arrow_key_flag;
                else
                    return undefined_key;
            } else if (combo == 'b') {
                return prev_word_key;
            } else if (combo == 'f') {
                return next_word_key;
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
std::pair<CmdExecResult, std::string> dvlab::CommandLineInterface::listen_to_input(std::istream& istr, std::string_view prompt, ListenConfig const& config) {
    using namespace key_code;
    auto const setting_restorer = dvlab::utils::scope_exit{
        [old_settings             = set_keypress(),
         old_listening_for_inputs = std::exchange(_listening_for_inputs, true),
         old_prompt               = std::exchange(_command_prompt, prompt),
         this]() {
            reset_keypress(old_settings);
            _command_prompt       = old_prompt;
            _listening_for_inputs = old_listening_for_inputs;
            if (_temp_command_stored) {
                _history.pop_back();
                _temp_command_stored = false;
            }
        }};

    _clear_read_buffer_and_print_prompt();

    while (true) {
        auto keycode = get_char(istr);

        if (istr.eof()) {
            return {CmdExecResult::done, std::string{dvlab::str::trim_spaces(dvlab::str::trim_comments(_read_buffer))}};
        }

        if (keycode == input_end_key) {
            return {CmdExecResult::quit, std::string{dvlab::str::trim_spaces(dvlab::str::trim_comments(_read_buffer))}};
        }

        switch (keycode) {
            case newline_key: {
                if (_dequote(_read_buffer).has_value()) {
                    return {CmdExecResult::done, std::string{dvlab::str::trim_spaces(dvlab::str::trim_comments(_read_buffer))}};
                } else {
                    _print_if_echo("\n{0:<{1}}", "...", _command_prompt.size());
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
                utils::clear_terminal();
                _reprint_command();
                break;
            case arrow_up_key:
                if (!config.allow_browse_history || _history_idx == 0) {
                    detail::beep();
                } else {
                    _retrieve_history(_prev_matching_history(1));
                }
                break;
            case arrow_down_key:
                (config.allow_browse_history) ? _retrieve_history(_next_matching_history(1)) : detail::beep();
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
                (config.allow_browse_history) ? _retrieve_history(_prev_matching_history(10)) : detail::beep();
                break;
            case pg_down_key:
                (config.allow_browse_history) ? _retrieve_history(_next_matching_history(10)) : detail::beep();
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
            case prev_word_key:
                _to_prev_word();
                break;
            case next_word_key:
                _to_next_word();
                break;
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
        _print_if_echo("{}", std::string(_cursor_position - pos, '\b'));
    }

    // move right
    if (_cursor_position < pos) {
        _print_if_echo("{}", _read_buffer.substr(_cursor_position, pos - _cursor_position));
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
    _print_if_echo("{}", _read_buffer.substr(_cursor_position + 1));  // move cursor to the end
    _print_if_echo(" \b");                                            // get rid of the last character

    _read_buffer.erase(_cursor_position, 1);

    auto const idx   = _cursor_position;
    _cursor_position = _read_buffer.size();  // before moving cursor, reflect the change in actual cursor location
    _move_cursor_to(idx);                    // move the cursor back to where it should be
    return true;
}

constexpr std::string_view word_chars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

bool dvlab::CommandLineInterface::_to_prev_word() {
    if (_cursor_position == 0) {
        detail::beep();
        return false;
    }
    auto const prev_word_end = _read_buffer.find_last_of(word_chars, _cursor_position - 1);
    if (prev_word_end == std::string::npos) {
        _move_cursor_to(0);
        return true;
    }
    auto const prev_word_begin = _read_buffer.find_last_not_of(word_chars, prev_word_end);
    if (prev_word_begin == std::string::npos) {
        _move_cursor_to(0);
        return true;
    }
    _move_cursor_to(prev_word_begin + 1);
    return true;
}

bool dvlab::CommandLineInterface::_to_next_word() {
    auto const next_space = _read_buffer.find_first_not_of(word_chars, _cursor_position);
    if (next_space == std::string::npos) {
        _move_cursor_to(_read_buffer.size());
    }
    auto const next_word_begin = _read_buffer.find_first_of(word_chars, next_space);
    if (next_word_begin == std::string::npos) {
        _move_cursor_to(next_space);
        return true;
    }
    _move_cursor_to(next_word_begin);
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
    _print_if_echo("{}", _read_buffer.substr(_cursor_position));
    auto const idx   = _cursor_position + 1;
    _cursor_position = _read_buffer.size();
    _move_cursor_to(idx);
}

/**
 * @brief delete the current line on the screen and reset read buffer
 *
 */
void dvlab::CommandLineInterface::_delete_line() {
    _move_cursor_to(_read_buffer.size());
    _print_if_echo("{}", std::string(_cursor_position, '\b'));
    _print_if_echo("{}", std::string(_cursor_position, ' '));
    _print_if_echo("{}", std::string(_cursor_position, '\b'));
    _read_buffer.clear();
}

/**
 * @brief Reprint the current command to a newline. Cursor should be restored to the original location.
 *
 */
void dvlab::CommandLineInterface::_reprint_command() {
    // NOTE - DON'T CHANGE - The logic here is as concise as it can be although seemingly redundant.
    auto const idx   = _cursor_position;
    _cursor_position = _read_buffer.size();  // before moving cursor, reflect the change in actual cursor location
    _print_if_echo("\n{}{}", _command_prompt, _read_buffer);
    _flush_output();
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
            _history.emplace_back(_read_buffer, CmdExecResult::done);
        } else if (_temp_command_stored && _history_idx == _history.size() - 1)
            _history.back().input = _read_buffer;  // => update it
    } else if (index > _history_idx) {             // move down
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

size_t dvlab::CommandLineInterface::_prev_matching_history(size_t count) {
    if (count == 0) return _history_idx;
    auto const prefix = _temp_command_stored ? _history.back().input : _read_buffer;
    size_t targ_idx   = _history_idx;
    for (auto i : std::views::iota(0ul, _history_idx) | std::views::reverse) {
        if (_history[i].input.starts_with(prefix)) {
            --count;
            targ_idx = i;
            if (count == 0) break;
        }
    }
    if (targ_idx == _history_idx)
        detail::beep();

    return targ_idx;
}

size_t dvlab::CommandLineInterface::_next_matching_history(size_t count) {
    if (count == 0) return _history_idx;
    auto const prefix = _temp_command_stored ? _history.back().input : _read_buffer;
    size_t targ_idx   = _history_idx;
    if (_history_idx == _history.size()) {
        assert(!_temp_command_stored);
        return _history_idx;
    }
    for (auto i : std::views::iota(_history_idx + 1, _history.size())) {
        if (_history[i].input.starts_with(prefix)) {
            --count;
            targ_idx = i;
            if (count == 0) break;
        }
    }
    if (targ_idx == _history_idx)
        detail::beep();

    return targ_idx;
}

/**
 * @brief Add the command in buffer to _history.
 *
 */
void dvlab::CommandLineInterface::_add_to_history(HistoryEntry const& entry) {
    _history.emplace_back(entry);
}

/**
 * @brief replace the read buffer with the latest history
 *
 */
void dvlab::CommandLineInterface::_replace_read_buffer_with_history() {
    if (_history_idx == _history.size()) {
        assert(!_temp_command_stored);
        return;
    }
    _delete_line();
    _read_buffer = _history[_history_idx].input;
    _print_if_echo("{}", _read_buffer);
    _cursor_position = _history[_history_idx].input.size();
    if (_temp_command_stored && _history_idx == _history.size() - 1) {
        _temp_command_stored = false;
        _history.pop_back();
    }
}

/**
 * @brief reset the read bufferm
 *
 */
void dvlab::CommandLineInterface::_clear_read_buffer_and_print_prompt() {
    _read_buffer.clear();
    _cursor_position = 0;
    _tab_press_count = 0;
    _print_if_echo("{}", _command_prompt);
    _flush_output();
}

void dvlab::CommandLineInterface::_replace_at_cursor(std::string_view old_str, std::string_view new_str) {
    if (_read_buffer.substr(_cursor_position - old_str.size(), old_str.size()) != std::string(old_str)) {
        spdlog::critical("word replacement failed: old string not matched");
        return;
    }
    _move_cursor_to(_cursor_position - old_str.size());
    for (size_t i = 0; i < old_str.size(); ++i) {
        _delete_char();
    }
    for (auto ch : new_str) {
        _insert_char(ch);
    }
}

}  // namespace dvlab
