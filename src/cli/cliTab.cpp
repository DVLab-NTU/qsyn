/****************************************************************************
  FileName     [ cliTab.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define the behavior on pressing tabs for CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/std.h>

#include <filesystem>
#include <ranges>
#include <regex>

#include "./cli.hpp"
#include "unicode/display_width.hpp"
#include "util/terminalAttributes.hpp"
#include "util/textFormat.hpp"
#include "util/util.hpp"

using namespace std;
namespace fs = std::filesystem;

void CommandLineInterface::onTabPressed() {
    assert(_tabPressCount != 0);
    std::string str = _readBuf.substr(0, _cursorPosition);
    str = stripComments(str);
    str = str.substr(str.find_last_of(';') + 1);
    str = stripLeadingWhitespaces(str);

    assert(str.empty() || str[0] != ' ');

    // if we are at the first token, we should list all commands and aliases
    bool listCommands = str.empty() || str.find_first_of(" =") == string::npos;
    if (listCommands) {
        matchCommandsAndAliases(str);
        return;
    }

    Command* cmd = getCommand(str.substr(0, str.find_first_of(' ')) /* first word*/);

    // [case 5] Singly matched on first tab
    if (cmd != nullptr && _tabPressCount == 1) {
        fmt::print("\n");
        cmd->usage();
        reprintCommand();

        return;
    }

    bool definingVariables = str.back() == '=';
    if (definingVariables) _tabPressCount = 0;

    assert(str[0] != ' ');
    if (auto result = matchVariables(str); result != NO_OP) {
        return;
    }

    if (auto result = matchFiles(str); result != NO_OP) {
        return;
    }

    // nothing to do
    beep();
}

CommandLineInterface::TabActionResult CommandLineInterface::matchCommandsAndAliases(std::string const& str) {
    _tabPressCount = 0;

    auto [matchBegin, matchEnd] = getCommandMatches(str);

    // [case 4] no matching cmd in the first word
    if (matchBegin == matchEnd) {
        beep();
        return NO_OP;
    }

    // cases 1, 2, 3 go here
    // [case 3] single command; insert ' '
    if (std::next(matchBegin) == matchEnd) {
        string ss = matchBegin->first + matchBegin->second->getOptCmd();
        for (size_t i = str.size(); i < ss.size(); ++i)
            insertChar(ss[i]);
        insertChar(' ');
        return AUTOCOMPLETE;
    }

    // [case 1, 2] multiple matches
    vector<string> words;

    for (auto itr = matchBegin; itr != matchEnd; ++itr) {
        auto const& [mand, cmd] = *itr;
        words.emplace_back(mand + cmd->getOptCmd());
    }

    printAsTable(words);
    reprintCommand();

    return LIST_OPTIONS;
}

std::pair<CommandLineInterface::CmdMap::const_iterator, CommandLineInterface::CmdMap::const_iterator>
CommandLineInterface::getCommandMatches(string const& str) const {
    string cmd = toUpperString(str);

    // all cmds
    if (cmd.empty()) return {_cmdMap.begin(), _cmdMap.end()};

    // singly matched
    if (getCommand(cmd)) {  // cmd is enough to determine a single cmd
        auto [bi, ei] = _cmdMap.equal_range(cmd);
        if (!_cmdMap.contains(cmd)) {
            --bi;
        }
        return {bi, ei};
    }

    // multiple matches / no matches
    string cmdNext = cmd;
    cmdNext.back()++;

    auto bi = _cmdMap.lower_bound(cmd);
    auto ei = _cmdMap.lower_bound(cmdNext);

    return {bi, ei};
}

CommandLineInterface::TabActionResult CommandLineInterface::matchVariables(std::string const& str) {
    // if there is a complete variable names trailing the string, replace it with its values
    static const regex var_matcher(R"((\$[\w]+$|\$\{[\w]+\}$))");
    std::smatch matches;

    if (std::regex_search(str, matches, var_matcher) && matches.prefix().str().back() != '\\') {
        string var = matches[0];
        bool is_brace = (var[1] == '{');
        string var_key = is_brace ? var.substr(2, var.size() - 3) : var.substr(1);
        if (_variables.contains(var_key)) {
            string val = _variables.at(var_key);

            size_t pos = _readBuf.find(var);
            moveCursor(_readBuf.size());
            moveCursor(pos);

            for (size_t i = 0; i < var.size(); ++i) {
                deleteChar();
            }

            for (auto ch : val) {
                insertChar(ch);
            }

            return AUTOCOMPLETE;
        }
    }

    // if there are incomplete variable name, tries to complete for the user
    static const regex var_prefix_matcher(R"(\$\{?[\w]*$)");

    if (!std::regex_search(str, matches, var_prefix_matcher) || matches.prefix().str().back() == '\\') return NO_OP;

    string var_prefix = matches[0];
    bool is_brace = (var_prefix[1] == '{');
    string var_key = var_prefix.substr(is_brace ? 2 : 1);

    std::vector<std::string> matching_variables;

    for (auto& [key, val] : _variables) {
        if (key.starts_with(var_key)) {
            matching_variables.emplace_back(key);
        }
    }

    if (matching_variables.size() == 0) {
        return NO_OP;
    }

    if (autocomplete(var_key, matching_variables, false /* does not matter */)) {
        if (matching_variables.size() == 1 && is_brace) {
            insertChar('}');
        }
        return AUTOCOMPLETE;
    }

    std::ranges::sort(matching_variables, [](std::string const& a, std::string const& b) { return toLowerString(a) < toLowerString(b); });

    printAsTable(matching_variables);

    reprintCommand();

    return LIST_OPTIONS;
}

CommandLineInterface::TabActionResult CommandLineInterface::matchFiles(std::string const& str) {
    std::optional<std::string> searchString;
    string incompleteQuotes;
    if (searchString = stripQuotes(str); searchString.has_value()) {
        incompleteQuotes = "";
    } else if (searchString = stripQuotes(str + "\""); searchString.has_value()) {
        incompleteQuotes = "\"";
    } else if (searchString = stripQuotes(str + "\'"); searchString.has_value()) {
        incompleteQuotes = "\'";
    } else {
        logger.fatal("unexpected quote stripping result!!");
        return NO_OP;
    }
    assert(searchString.has_value());

    size_t lastSpacePos = std::invoke(
        [&searchString]() -> size_t {
            size_t pos = searchString->find_last_of(" =");
            while (pos != string::npos && (*searchString)[pos - 1] == '\\') {
                pos = searchString->find_last_of(" =", pos - 2);
            }
            return pos;
        });
    assert(lastSpacePos != string::npos);  // there must be a ' ' or '=' in cmd

    auto filepath = fs::path(searchString->substr(lastSpacePos + 1));
    auto dirname = filepath.parent_path();
    auto prefix = filepath.filename().string();

    for (size_t i = 1; i < prefix.size(); ++i) {
        if (isSpecialChar(prefix[i])) {
            if (prefix[i - 1] == '\\') {
                prefix.erase(i - 1, 1);
                --i;
            }
        }
    }

    filepath = dirname / prefix;

    vector<string> files = getFileMatches(filepath);

    // no matched file
    if (files.size() == 0) {
        return NO_OP;
    }

    if (autocomplete(prefix, files, incompleteQuotes.size())) {
        if (files.size() == 1) {
            if (fs::is_directory(dirname / files[0])) {
                insertChar('/');
            } else {
                if (!incompleteQuotes.empty()) insertChar(incompleteQuotes[0]);
                insertChar(' ');
            }
        }

        return AUTOCOMPLETE;
    }

    // no auto complete : list matched files

    for (auto& file : files) {
        for (size_t i = 0; i < file.size(); ++i) {
            if (isSpecialChar(file[i])) {
                file.insert(i, "\\");
                ++i;
            }
        }
    }

    for (auto& file : files) {
        using namespace dvlab;
        file = fmt::format("{}", fmt_ext::styled_if_ANSI_supported(file, fmt_ext::ls_color(dirname / file)));
    }

    printAsTable(files);

    reprintCommand();

    return LIST_OPTIONS;
}

/**
 * @brief returns the all filenames the matches `path`. Note that directory name is not contained in the returned list!
 *
 * @param path
 * @return vector<string>
 */
vector<string> CommandLineInterface::getFileMatches(fs::path const& path) const {
    vector<string> files;

    auto dirname = path.parent_path().empty() ? "." : path.parent_path();
    auto prefix = path.filename().string();

    if (!fs::exists(dirname)) {
        logger.error("Error: failed to open {}!!", path.parent_path());
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
        std::erase_if(files, [this, &prefix](string const& file) { return !isSpecialChar(file[prefix.size()]); });
    }

    // don't show hidden files
    std::erase_if(files, [](std::string const& file) { return file.starts_with("."); });

    std::ranges::sort(files, [](std::string const& a, std::string const& b) { return toLowerString(a) < toLowerString(b); });

    return files;
}

/**
 * @brief complete the input by the common characters of a list of strings for user.
 *
 * @param prefix The part of strings that is already on console
 * @param strs The strings to autocomplete the common characters for
 * @param inQuotes Whether the prefix is in a pair of quotes
 * @return true if able to autocomplete
 * @return false if not
 */
bool CommandLineInterface::autocomplete(std::string prefixCopy, std::vector<std::string> const& strs, bool inQuotes) {
    if (strs.size() == 1 && prefixCopy == strs[0]) return true;  // edge case: completing a file/dir name that is already complete
    bool trailingBackslash = false;
    if (prefixCopy.back() == '\\') {
        prefixCopy.pop_back();
        trailingBackslash = true;
    }
    assert(std::ranges::all_of(strs, [&prefixCopy](std::string const& str) { return str.starts_with(prefixCopy); }));

    size_t shortestFileLen = std::ranges::min(
        strs | std::views::transform([](std::string const& str) { return str.size(); }));

    string autocompleteStr = "";
    for (size_t i = prefixCopy.size(); i < shortestFileLen; ++i) {
        if (std::ranges::adjacent_find(strs, [&i](std::string const& a, std::string const& b) {
                return a[i] != b[i];
            }) != strs.end()) break;
        // if outside of a pair of quote, prepend special characters in the file/dir name with backslash
        if (isSpecialChar(strs[0][i]) && !inQuotes) {
            autocompleteStr += '\\';
        }
        autocompleteStr += strs[0][i];
    }

    if (autocompleteStr.empty()) return false;

    // if the original string terminates with a backslash, and the string to complete starts with '\',
    // the completion should start from the position of backslash
    // suppose completing a\ b.txt
    // > somecmd a\_[Tab] --> autoCompleteStr = "\ b.txt"
    // > somecmd a\ b.txt <-- should complete like this
    if (trailingBackslash && autocompleteStr[0] == '\\') {
        autocompleteStr = autocompleteStr.substr(1);
    }

    for (auto& ch : autocompleteStr) {
        insertChar(ch);
    }

    return autocompleteStr.size() > 0;
}

void CommandLineInterface::printAsTable(std::vector<std::string> words) const {
    // calculate an lower bound to the spacing first
    fort::utf8_table table;
    table.set_border_style(FT_EMPTY_STYLE);
    table.set_cell_left_padding(0);
    table.set_cell_right_padding(2);

    ft_set_u8strwid_func(
        [](void const* beg, void const* end, size_t* width) -> int {
            std::string tmpStr(static_cast<const char*>(beg), static_cast<const char*>(end));

            *width = unicode::display_width(tmpStr);

            return 0;
        });

    auto longest_word_len = std::ranges::max(
        words | std::views::transform([](std::string const& str) { return unicode::display_width(str); }));

    size_t num_columns = dvlab::utils::get_terminal_size().width / (longest_word_len + 2);
    size_t num_rows = 1 + (words.size() - 1) / num_columns;

    for (size_t i = 0; i < num_rows; ++i) {
        for (size_t j = i; j < words.size(); j += num_rows) {
            table << words[j];
        }
        table << fort::endr;
    }
    fmt::print("\n{}", table.to_string());
}