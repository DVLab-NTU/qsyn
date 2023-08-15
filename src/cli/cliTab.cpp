/****************************************************************************
  FileName     [ cliTab.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define the behavior on pressing tabs for CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/std.h>

#include <filesystem>

#include "./cli.hpp"
#include "unicode/display_width.hpp"
#include "util/terminalAttributes.hpp"
#include "util/textFormat.hpp"

using namespace std;
namespace fs = std::filesystem;

void CommandLineInterface::matchAndComplete(const string& str) {
    assert(str.empty() || str[0] != ' ');

    if (str.size()) {
        assert(!str.empty());

        if (size_t firstSpacePos = str.find_first_of(' '); firstSpacePos != string::npos) {  // already has ' '; Cursor NOT on first word
            assert(_tabPressCount != 0);
            CmdExec* e = getCmd(str.substr(0, firstSpacePos));  // first word

            // [case 6] Singly matched on second+ tab
            // [case 7] no match; cursor not on first word
            if (e == nullptr || (_tabPressCount > 1 && !matchFilesAndComplete(str))) {
                beep();
                return;
            }

            // [case 5] Singly matched on first tab
            else if (_tabPressCount == 1) {
                fmt::print("\n");
                e->usage();
            }

            reprintCmd();
            return;  // from cases 5, 6, 7
        }
    }  // end of cmd string processing

    _tabPressCount = 0;

    auto [matchBegin, matchEnd] = getCmdMatches(str);

    // [case 4] no matching cmd in the first word
    if (matchBegin == matchEnd) {
        beep();
        return;
    }

    // cases 1, 2, 3 go here
    // [case 3] single command; insert ' '
    if (std::next(matchBegin) == matchEnd) {
        string ss = matchBegin->first + matchBegin->second->getOptCmd();
        for (size_t i = str.size(); i < ss.size(); ++i)
            insertChar(ss[i]);
        insertChar(' ');
        return;
    }

    // [case 1, 2] multiple matches
    vector<string> words;

    for (auto itr = matchBegin; itr != matchEnd; ++itr) {
        auto const& [mand, cmd] = *itr;
        words.emplace_back(mand + cmd->getOptCmd());
    }

    printAsTable(words);
    reprintCmd();
}

std::pair<CommandLineInterface::CmdMap::const_iterator, CommandLineInterface::CmdMap::const_iterator>
CommandLineInterface::getCmdMatches(string const& str) {
    string cmd = toUpperString(str);

    // all cmds
    if (cmd.empty()) return {_cmdMap.begin(), _cmdMap.end()};

    // singly matched
    if (getCmd(cmd)) {  // cmd is enough to determine a single cmd
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

/**
 * @brief returns the all filenames the matches `path`. Note that directory name is not contained in the returned list!
 *
 * @param path
 * @return vector<string>
 */
vector<string> CommandLineInterface::getMatchedFiles(fs::path const& path) const {
    vector<string> files;

    auto dirname = path.parent_path().empty() ? "." : path.parent_path();
    auto prefix = path.filename().string();

    if (!fs::exists(dirname)) {
        fmt::println(stderr, "Error: failed to open {}!!", path.parent_path());
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
 * @param hasTrailingBackslash
 * @return true if some characters are auto-inserted, and
 * @return false if not
 */
bool CommandLineInterface::completeCommonChars(std::string prefixCopy, std::vector<std::string> const& strs, bool inQuotes) {
    bool trailingBackslash = false;
    if (prefixCopy.back() == '\\') {
        prefixCopy.pop_back();
        trailingBackslash = true;
    }
    assert(std::ranges::all_of(strs, [&prefixCopy](std::string const& str) { return str.starts_with(prefixCopy); }));

    size_t shortestFileLen = std::ranges::min(
        strs | std::views::transform([](std::string const& str) { return str.size(); }));

    string autocompleteStr = "";
    for (size_t i : std::views::iota(prefixCopy.size(), shortestFileLen)) {
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

/**
 * @brief list the files that match the last word in `cmd`.
 *
 * @param cmd the command line with leading ' ' removed; there must be a ' ' in cmd
 * @return true if printing files
 * @return false if completing (part of) the word
 */
bool CommandLineInterface::matchFilesAndComplete(const string& cmd) {
    assert(cmd[0] != ' ');
    std::optional<std::string> searchString;
    string incompleteQuotes;
    if (searchString = stripQuotes(cmd); searchString.has_value()) {
        incompleteQuotes = "";
    } else if (searchString = stripQuotes(cmd + "\""); searchString.has_value()) {
        incompleteQuotes = "\"";
    } else if (searchString = stripQuotes(cmd + "\'"); searchString.has_value()) {
        incompleteQuotes = "\'";
    } else {
        fmt::println(stderr, "Error: unexpected quote stripping result!!");
        return false;
    }
    assert(searchString.has_value());

    size_t lastSpacePos = std::invoke(
        [&searchString]() -> size_t {
            size_t pos = searchString->find_last_of(" ");
            while (pos != string::npos && (*searchString)[pos - 1] == '\\') {
                pos = searchString->find_last_of(" ", pos - 2);
            }
            return pos;
        });
    assert(lastSpacePos != string::npos);  // must have ' '

    searchString = searchString->substr(lastSpacePos + 1);

    auto filepath = fs::path(*searchString);
    auto dirname = filepath.parent_path();
    auto prefix = filepath.filename().string();

    vector<string> files = getMatchedFiles(filepath);

    // no matched file
    if (files.size() == 0) {
        return false;
    }

    if (completeCommonChars(prefix, files, incompleteQuotes.size())) {
        if (files.size() == 1) {
            if (fs::is_directory(dirname / files[0])) {
                insertChar('/');
            } else {
                if (!incompleteQuotes.empty()) insertChar(incompleteQuotes[0]);
                insertChar(' ');
            }
        }

        return false;
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
        using namespace dvlab_utils;
        file = fmt::format("{}", fmt_ext::styled_if_ANSI_supported(file, fmt_ext::ls_color(dirname / file)));
    }

    printAsTable(files);

    return true;
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

    size_t num_columns = dvlab_utils::get_terminal_size().width / (longest_word_len + 2);
    size_t num_rows = 1 + (words.size() - 1) / num_columns;

    for (size_t i = 0; i < num_rows; ++i) {
        for (size_t j = i; j < words.size(); j += num_rows) {
            table << words[j];
        }
        table << fort::endr;
    }
    fmt::print("\n{}", table.to_string());
}