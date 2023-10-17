/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define the behavior on pressing tabs for dvlab::CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fort.hpp>
#include <ranges>
#include <regex>

#include "./cli.hpp"
#include "fmt/color.h"
#include "unicode/display_width.hpp"
#include "util/terminal_attributes.hpp"
#include "util/text_format.hpp"
#include "util/util.hpp"

namespace fs = std::filesystem;

namespace {
bool string_case_insensitive_less_than(std::string const& a, std::string const& b) {
    return dvlab::str::tolower_string(a) < dvlab::str::tolower_string(b);
}

}  // namespace

namespace dvlab {

/**
 * @brief define tab press action, which tries to complete the input for the user.
 *
 */
void dvlab::CommandLineInterface::_on_tab_pressed() {
    assert(_tab_press_count != 0);

    auto str = _read_buffer.substr(0, _cursor_position);

    size_t last_token_pos  = _get_last_token_pos(str);
    std::string last_token = last_token_pos != std::string::npos ? _read_buffer.substr(last_token_pos, _cursor_position - last_token_pos) : "";
    assert(last_token.empty() || last_token[0] != ' ');

    // if this is the first token in the command, try to match identifiers first
    auto last_cmd_pos = _read_buffer.find_first_not_of(' ', _get_last_token_pos(str, ';'));
    if (last_cmd_pos == std::string::npos) last_cmd_pos = 0;
    if (last_cmd_pos == last_token_pos) {
        if (auto result = _match_identifiers(last_token); result != TabActionResult::no_op) {
            return;
        }
    }

    auto first_space_pos_in_last_cmd = _read_buffer.find_first_of(' ', last_cmd_pos);
    while (first_space_pos_in_last_cmd != std::string::npos && _is_escaped(_read_buffer, first_space_pos_in_last_cmd)) {
        first_space_pos_in_last_cmd = _read_buffer.find_first_of(' ', first_space_pos_in_last_cmd + 1);
    }
    dvlab::Command* cmd = get_command(str.substr(last_cmd_pos, first_space_pos_in_last_cmd - last_cmd_pos) /* first word*/);

    // [case 5] Singly matched on first tab
    if (cmd != nullptr && _tab_press_count == 1) {
        fmt::println("");
        cmd->print_usage();
        _reprint_command();

        return;
    }

    assert(last_token[0] != ' ');
    if (auto result = _match_variables(str); result != TabActionResult::no_op) {
        return;
    }

    if (auto result = _match_files(str); result != TabActionResult::no_op) {
        return;
    }

    // nothing to do
    detail::beep();
}

dvlab::CommandLineInterface::TabActionResult dvlab::CommandLineInterface::_match_identifiers(std::string const& str) {
    _tab_press_count = 0;

    auto matches = _identifiers.find_all_with_prefix(str);

    std::ranges::sort(matches);

    // [case 4] no matching cmd/alias in the first word
    if (matches.empty()) {
        detail::beep();
        return TabActionResult::no_op;
    }

    // cases 1, 2, 3 go here
    // [case 3] single command; insert ' '
    if (matches.size() == 1) {
        // if completely matched an alias, replace it with its replacement string and insert a space
        if (matches[0].size() == str.size() && _aliases.contains(str)) {
            _replace_at_cursor(str, _aliases.at(str));
        } else {
            std::for_each(dvlab::iterator::next(matches[0].begin(), str.size()), matches[0].end(), [this](char ch) { _insert_char(ch); });
            // if the matched command is not an alias, insert a space
            // if it is, we want to allow for further completion
            // unless the alias is identical to its replacement string (gosh, why would you do that?),
            // where we want to insert a space to avoid endless alias expansion
            if (!_aliases.contains(matches[0]) || _aliases.at(matches[0]) == matches[0]) {
                _insert_char(' ');
            }
        }

        return TabActionResult::autocomplete;
    }

    // [case 1, 2] multiple matches
    _print_as_table(matches);
    _reprint_command();

    return TabActionResult::list_options;
}

dvlab::CommandLineInterface::TabActionResult dvlab::CommandLineInterface::_match_variables(std::string const& str) {
    // if there is a complete variable names trailing the string, replace it with its values
    static const std::regex var_matcher(R"((\$[\w]+$|\$\{[\w]+\}$))");
    std::smatch matches;

    if (std::regex_search(str, matches, var_matcher) && matches.prefix().str().back() != '\\') {
        std::string var     = matches[0];
        bool is_brace       = (var[1] == '{');
        std::string var_key = is_brace ? var.substr(2, var.size() - 3) : var.substr(1);
        if (_variables.contains(var_key)) {
            if (_is_escaped(str, matches.position(0))) {
                return TabActionResult::no_op;
            }
            _replace_at_cursor(var, _variables.at(var_key));

            return TabActionResult::autocomplete;
        }
    }

    // if there are incomplete variable name, tries to complete for the user
    static const std::regex var_prefix_matcher(R"(\$\{?[\w]*$)");

    if (!std::regex_search(str, matches, var_prefix_matcher) || matches.prefix().str().back() == '\\') {
        return TabActionResult::no_op;
    }

    std::string var_prefix = matches[0];
    if (_is_escaped(str, str.find(var_prefix))) {
        return TabActionResult::no_op;
    }
    bool is_brace       = (var_prefix[1] == '{');
    std::string var_key = var_prefix.substr(is_brace ? 2 : 1);

    std::vector<std::string> matching_variables;

    for (auto& [key, val] : _variables) {
        if (key.starts_with(var_key)) {
            matching_variables.emplace_back(key);
        }
    }

    if (matching_variables.size() == 0) {
        return TabActionResult::no_op;
    }

    if (_autocomplete(var_key, matching_variables, parse_state::normal /* does not matter */)) {
        if (matching_variables.size() == 1 && is_brace) {
            _insert_char('}');
        }
        return TabActionResult::autocomplete;
    }

    std::ranges::sort(matching_variables, string_case_insensitive_less_than);

    _print_as_table(matching_variables);

    _reprint_command();

    return TabActionResult::list_options;
}

dvlab::CommandLineInterface::TabActionResult dvlab::CommandLineInterface::_match_files(std::string const& str) {
    std::optional<std::string> search_string;
    parse_state state = parse_state::normal;
    if (search_string = _dequote(str); search_string.has_value()) {
        state = parse_state::normal;
    } else if (search_string = _dequote(str + "\""); search_string.has_value()) {
        state = parse_state::double_quote;
    } else if (search_string = _dequote(str + "\'"); search_string.has_value()) {
        state = parse_state::single_quote;
    } else {
        spdlog::critical("unexpected dequote result!!");
        return TabActionResult::no_op;
    }
    assert(search_string.has_value());
    auto last_space_pos = _get_last_token_pos(*search_string);
    assert(last_space_pos != std::string::npos);  // there must be a ' ' in cmd

    auto filepath = fs::path(search_string->substr(last_space_pos));
    auto dirname  = filepath.parent_path();
    auto prefix   = filepath.filename().string();

    for (size_t i = 1; i < prefix.size(); ++i) {
        if (_is_special_char(prefix[i])) {
            if (prefix[i - 1] == '\\') {
                prefix.erase(i - 1, 1);
                --i;
            }
        }
    }

    filepath = dirname / prefix;

    std::vector<std::string> files = _get_file_matches(filepath);

    // no matched file
    if (files.size() == 0) {
        return TabActionResult::no_op;
    }

    if (_autocomplete(prefix, files, state)) {
        if (files.size() == 1) {
            if (fs::is_directory(dirname / files[0])) {
                _insert_char('/');
            } else {
                if (state == parse_state::single_quote) _insert_char('\'');
                if (state == parse_state::double_quote) _insert_char('\"');
                _insert_char(' ');
            }
        }

        return TabActionResult::autocomplete;
    }

    // no auto complete : list matched files

    for (auto& file : files) {
        for (size_t i = 0; i < file.size(); ++i) {
            if (_is_special_char(file[i])) {
                file.insert(i, "\\");
                ++i;
            }
        }
    }

    for (auto& file : files) {
        using namespace dvlab;
        file = fmt::format("{}", fmt_ext::styled_if_ansi_supported(file, fmt_ext::ls_color(dirname / file)));
    }

    _print_as_table(files);

    _reprint_command();

    return TabActionResult::list_options;
}

/**
 * @brief returns the all filenames the matches `path`. Note that directory name is not contained in the returned list!
 *
 * @param path
 * @return vector<string>
 */
std::vector<std::string> dvlab::CommandLineInterface::_get_file_matches(fs::path const& path) const {
    std::vector<std::string> files;

    auto dirname = path.parent_path().empty() ? "." : path.parent_path();
    auto prefix  = path.filename().string();

    if (!fs::exists(dirname)) {
        spdlog::error("failed to open {}!!", path.parent_path());
        return files;
    }

    for (auto& entry : fs::directory_iterator(dirname)) {
        if (prefix.back() == '\\')
            prefix.pop_back();
        if (prefix.empty() || entry.path().filename().string().compare(0, prefix.size(), prefix) == 0) {
            files.emplace_back(entry.path().filename());
        }
    }

    if (prefix.back() == '\\') {
        std::erase_if(files, [this, &prefix](std::string const& file) { return !_is_special_char(file[prefix.size()]); });
    }

    // don't show hidden files
    std::erase_if(files, [](std::string const& file) { return file.starts_with("."); });

    std::ranges::sort(files, string_case_insensitive_less_than);

    return files;
}

/**
 * @brief complete the input by the common characters of a list of strings for user.
 *
 * @param prefix The part of strings that is already on terminal
 * @param strs The strings to autocomplete the common characters for
 * @param inQuotes Whether the prefix is in a pair of quotes
 * @return true if able to autocomplete
 * @return false if not
 */
bool dvlab::CommandLineInterface::_autocomplete(std::string prefix_copy, std::vector<std::string> const& strs, parse_state state) {
    if (strs.size() == 1 && prefix_copy == strs[0]) return true;  // edge case: completing a file/dir name that is already complete
    bool trailing_backslash = false;
    if (prefix_copy.back() == '\\') {
        prefix_copy.pop_back();
        trailing_backslash = true;
    }
    assert(std::ranges::all_of(strs, [&prefix_copy](std::string const& str) { return str.starts_with(prefix_copy); }));

    size_t shortest_file_len = std::ranges::min(
        strs | std::views::transform([](std::string const& str) { return str.size(); }));

    std::string autocomplete_str = "";
    for (size_t i = prefix_copy.size(); i < shortest_file_len; ++i) {
        if (std::ranges::adjacent_find(strs, [&i](std::string const& a, std::string const& b) {
                return a[i] != b[i];
            }) != strs.end()) break;
        // if outside of a pair of quote, prepend special characters in the file/dir name with backslash
        if (_is_special_char(strs[0][i]) && !_should_be_escaped(strs[0][i], state)) {
            autocomplete_str += '\\';
        }
        autocomplete_str += strs[0][i];
    }

    if (autocomplete_str.empty()) return false;

    // if the original string terminates with a backslash, and the string to complete starts with '\',
    // the completion should start from the position of backslash
    // suppose completing a\ b.txt
    // > somecmd a\_[Tab] --> autoCompleteStr = "\ b.txt"
    // > somecmd a\ b.txt <-- should complete like this
    if (trailing_backslash && autocomplete_str[0] == '\\') {
        autocomplete_str = autocomplete_str.substr(1);
    }

    for (auto& ch : autocomplete_str) {
        _insert_char(ch);
    }

    return autocomplete_str.size() > 0;
}

void dvlab::CommandLineInterface::_print_as_table(std::vector<std::string> words) const {
    // calculate an lower bound to the spacing first
    fort::utf8_table table;
    table.set_border_style(FT_EMPTY_STYLE);
    table.set_cell_left_padding(0);
    table.set_cell_right_padding(2);

    ft_set_u8strwid_func(
        [](void const* beg, void const* end, size_t* width) -> int {
            std::string tmp_str(static_cast<const char*>(beg), static_cast<const char*>(end));

            *width = unicode::display_width(tmp_str);

            return 0;
        });

    auto longest_word_len = std::ranges::max(
        words | std::views::transform([](std::string const& str) { return unicode::display_width(str); }));

    size_t num_columns = dvlab::utils::get_terminal_size().width / (longest_word_len + 2);
    size_t num_rows    = 1 + (words.size() - 1) / num_columns;

    for (size_t i = 0; i < num_rows; ++i) {
        for (size_t j = i; j < words.size(); j += num_rows) {
            table << words[j];
        }
        table << fort::endr;
    }
    fmt::print("\n{}", table.to_string());
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