/****************************************************************************
  FileName     [ cliParser.cpp ]
  PackageName  [ cli ]
  Synopsis     [ Define command parsing member functions for class CmdParser ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <thread>

#include "cli/cli.h"
#include "fort.hpp"
#include "unicode/display_width.hpp"
#include "util/terminalSize.h"
#include "util/textFormat.h"
#include "util/util.h"

using std::cout, std::endl, std::cerr;
using std::string, std::vector;
namespace fs = std::filesystem;

//----------------------------------------------------------------------
//    Member Function for class cmdParser
//----------------------------------------------------------------------
// return false if file cannot be opened
// Please refer to the comments in "DofileCmd::exec", cmdCommon.cpp
bool CommandLineInterface::openDofile(const std::string& dof) {
    constexpr size_t dofile_stack_limit = 1024;
    if (this->stop_requested()) {
        return false;
    }
    if (_dofileStack.size() >= dofile_stack_limit) {
        cerr << "Error: dofile stack overflow (" << dofile_stack_limit
             << ")" << endl;
        return false;
    }

    _dofileStack.push(std::move(std::ifstream(dof)));

    if (!_dofileStack.top().is_open()) {
        closeDofile();
        return false;
    }
    return true;
}

void CommandLineInterface::closeDofile() {
    assert(_dofileStack.size());
    _dofileStack.pop();
}

// Return false if registration fails
bool CommandLineInterface::regCmd(const string& cmd, unsigned nCmp, std::unique_ptr<CmdExec>&& e) {
    // Make sure cmd hasn't been registered and won't cause ambiguity
    string str = cmd;
    unsigned s = str.size();
    if (!e->initialize()) return false;
    if (s < nCmp) return false;
    while (true) {
        if (getCmd(str)) return false;
        if (s == nCmp) break;
        str.resize(--s);
    }
    // Change the first nCmp characters to upper case to facilitate
    //    case-insensitive comparison later.
    // The strings stored in _cmdMap are all upper case
    //

    // Guard: mandatory part cannot be subsets to another
    // Currently turned off (until maybe one day the alias system is in place)

    // for (const auto& [key, _] : _cmdMap) {
    //     if (key.find(str) != string::npos || str.find(key) != string::npos) {
    //         return false;
    //     }
    // }
    assert(str.size() == nCmp);  // str is now mandCmd
    string& mandCmd = str;
    for (unsigned i = 0; i < nCmp; ++i)
        mandCmd[i] = toupper(mandCmd[i]);
    string optCmd = cmd.substr(nCmp);
    assert(e != 0);
    e->setOptCmd(optCmd);

    // insert (mandCmd, e) to _cmdMap; return false if insertion fails.
    return (_cmdMap.insert(CmdRegPair(mandCmd, std::move(e)))).second;
}

void CommandLineInterface::sigintHandler(int signum) {
    if (_currCmd.has_value()) {
        // there is an executing command
        _currCmd->request_stop();
        cout << "Command Interrupted" << endl;
    } else {
        // receiving inputs
        cout << char(KeyCode::NEWLINE_KEY);
        resetBufAndPrintPrompt();
    }
}

// Return false on "quit" or if exception happens
CmdExecResult
CommandLineInterface::executeOneLine() {
    bool newCmd = false;
    while (_dofileStack.size() && _dofileStack.top().eof()) closeDofile();

    if (_dofileStack.size())
        newCmd = readCmd(_dofileStack.top());
    else
        newCmd = readCmd(std::cin);

    // execute the command
    if (!newCmd) return CmdExecResult::NOP;

    std::atomic<CmdExecResult> result;

    while (_commandQueue.size()) {
        auto [e, option] = parseOneCommandFromQueue();

        if (e == nullptr) continue;

        _currCmd = jthread::jthread(
            [this, &e = e, &option = option, &result]() {
                result = e->exec(option);
            });

        assert(_currCmd.has_value());

        _currCmd->join();

        if (this->stop_requested()) {
            cerr << "Command interrupted " << endl;
            while (_commandQueue.size()) _commandQueue.pop();
            return CmdExecResult::INTERRUPTED;
        }

        _currCmd = std::nullopt;
    }

    return result;
}

// For each CmdExec* in _cmdMap, call its "help()" to print out the help msg.
// Print an endl at the end.
void CommandLineInterface::printHelps() const {
    for (const auto& mi : _cmdMap)
        mi.second->summary();

    cout << endl;
}

void CommandLineInterface::printHistory() const {
    printHistory(_history.size());
}

void CommandLineInterface::printHistory(size_t nPrint) const {
    assert(_tempCmdStored == false);
    if (_history.empty()) {
        cout << "Empty command history!!" << endl;
        return;
    }
    size_t s = _history.size();
    for (auto i = s - std::min(s, nPrint); i < s; ++i)
        cout << "   " << i << ": " << _history[i] << endl;
}

//
// Parse the command from _history.back();
// Let string str = _history.back();
//
// 1. Read the command string (may contain multiple words) from the leading
//    part of str (i.e. the first word) and retrive the corresponding
//    CmdExec* from _cmdMap
//    ==> If command not found, print to cerr the following message:
//        Illegal command!! "(string cmdName)"
//    ==> return it at the end.
// 2. Call getCmd(cmd) to retrieve command from _cmdMap.
//    "cmd" is the first word of "str".
// 3. Get the command options from the trailing part of str (i.e. second
//    words and beyond) and store them in "option"
//
std::pair<CmdExec*, std::string>
CommandLineInterface::parseOneCommandFromQueue() {
    assert(_tempCmdStored == false);
    string buffer = _commandQueue.front();
    _commandQueue.pop();

    assert(buffer[0] != '\0' && buffer[0] != ' ');

    string cmd;
    size_t n = myStrGetTok2(buffer, cmd);
    CmdExec* e = getCmd(cmd);
    string option;
    if (!e) {
        cerr << "Illegal command!! (" << cmd << ")" << endl;
    } else if (n != string::npos) {
        option = buffer.substr(n);
    }
    return {e, option};
}

string CommandLineInterface::replaceVariableKeysWithValues(string const& str) const {
    // if `str` contains the some dollar sign '$',
    // try to convert it into variable
    // unless it is preceded by '\'.

    // Variables are in the form of `$NAME` or `${NAME}`,
    // where the name should consists of only alphabets, numbers,
    // and the underscore '_'.

    // If curly braces are used (${NAME}),
    // the text inside the curly braces is the variable name.

    // If otherwise no curly braces are used ($NAME),
    // the variable name is until some illegal characters for a name appears.

    // if a variable is existent, replace the $NAME or ${NAME} syntax with their value. Otherwise, replace the syntax with an empty string

    // e.g., suppose foo_bar=apple, foo=banana
    //       "$foo_bar"     --> "apple"
    //       "$foo.bar"     --> "banana.bar"
    //       "${foo}_bar"   --> "banana_bar"
    //       "foo_$bar"     --> "foo_"
    //       "${foo}${bar}" --> "banana"

    // optional: if inside ${NAME} is an illegal name string,
    // warn the user.

    std::vector<std::tuple<size_t, size_t, string>> to_replace;
    // \S means non-whitespace character
    static std::regex const var_without_braces(R"(\$[a-zA-Z0-9_]+)");
    static std::regex const var_with_braces(R"(\$\{\S+\})");
    for (auto re : {var_without_braces, var_with_braces}) {
        std::smatch matches;
        std::regex_search(str, matches, re);
        for (size_t i = 0; i < matches.size(); ++i) {
            string var = matches[i];
            // tell if it is a curly brace variable or not
            bool is_brace = var[1] == '{';
            string var_key = is_brace ? var.substr(2, var.length() - 3) : var.substr(1);

            bool is_defined = _variables.contains(var_key);
            string val = is_defined ? _variables.at(var_key) : "";

            if (is_brace && !is_defined) {
                for (auto ch : var_key) {
                    if (isalnum(ch) || ch == '_') {
                        continue;
                    }
                    cerr << "Warning: variable name `" << var_key << "` is illegal" << endl;
                    break;
                }
            }

            size_t pos = matches.position(i);
            if (isEscapedChar(str, pos)) {
                continue;
            }
            to_replace.emplace_back(pos, var.length(), val);
        }
    }

    size_t cursor = 0;
    string result = "";
    for (auto [pos, len, val] : to_replace) {
        result += str.substr(cursor, pos - cursor);
        result += val;
        cursor = pos + len;
    }
    result += str.substr(cursor);

    // return a string with all variables substituted with their value.
    return result;
}

//
// This function is called by pressing 'Tab'.
// It is to list the partially matched commands.
// "str" is the partial string before current cursor position. It can be
// a null string, or begin with ' '. The beginning ' ' will be ignored.
//
// Several possibilities after pressing 'Tab'
// (Let $ be the cursor position)
// 1. LIST ALL COMMANDS
//    --- 1.1 ---
//    [Before] Null cmd
//    cmd> $
//    --- 1.2 ---
//    [Before] Cmd with ' ' only
//    cmd>     $
//    [After Tab]
//    ==> List all the commands, each command is printed out by:
//           cout << setw(12) << left << cmd;
//    ==> Print a new line for every 5 commands
//    ==> After printing, re-print the prompt and place the cursor back to
//        original location (including ' ')
//    [TODO] FIXED by Ric @ 10/22/2018
//    ==> and reset _tabPressCount to 0
//
// 2. LIST ALL PARTIALLY MATCHED COMMANDS
//    --- 2.1 ---
//    [Before] partially matched (multiple matches)
//    cmd> h$                   // partially matched
//    [After Tab]
//    HELp        HIStory       // List all the parially matched commands
//    cmd> h$                   // and then re-print the partial command
//    --- 2.2 ---
//    [Before] partially matched (multiple matches)
//    cmd> h$llo                // partially matched with trailing characters
//    [After Tab]
//    HELp        HIStory       // List all the parially matched commands
//    cmd> h$llo                // and then re-print the partial command
//    [TODO] FIXED by Ric @ 10/22/2018
//    ==> and reset _tabPressCount to 0
//
// 3. LIST THE SINGLY MATCHED COMMAND
//    ==> In either of the following cases, print out cmd + ' '
//    ==> and reset _tabPressCount to 0
//    --- 3.1 ---
//    [Before] partially matched (single match)
//    cmd> he$
//    [After Tab]
//    cmd> heLp $               // auto completed with a space inserted
//    --- 3.2 ---
//    [Before] partially matched with trailing characters (single match)
//    cmd> he$ahah
//    [After Tab]
//    cmd> heLp $ahaha
//    ==> Automatically complete on the same line
//    ==> The auto-expanded part follow the strings stored in cmd map and
//        cmd->_optCmd. Insert a space after "heLp"
//    --- 3.3 ---
//    [Before] fully matched (cursor right behind cmd)
//    cmd> hElP$sdf
//    [After Tab]
//    cmd> hElP $sdf            // a space character is inserted
//
// 4. NO MATCH IN FITST WORD
//    --- 4.1 ---
//    [Before] No match
//    cmd> hek$
//    [After Tab]
//    ==> Beep and stay in the same location
//
// 5. FIRST WORD ALREADY MATCHED ON FIRST TAB PRESSING
//    --- 5.1 ---
//    [Before] Already matched on first tab pressing
//    cmd> help asd$gh
//    [After] Print out the usage for the already matched command
//    Usage: HELp [(string cmd)]
//    cmd> help asd$gh
//
// 6. FIRST WORD ALREADY MATCHED ON SECOND AND LATER TAB PRESSING
//    ==> Note: command usage has been printed under first tab press
//    ==> Check the word the cursor is at; get the prefix before the cursor
//    ==> So, this is to list the file names under current directory that
//        match the prefix
//    ==> List all the matched file names alphabetically by:
//           cout << setw(16) << left << fileName;
//    ==> Print a new line for every 5 commands
//    ==> After printing, re-print the prompt and place the cursor back to
//        original location
//    [TODO] FIXED by Ric @ 10/13/2018 --- 6.1 ---
//    Considering the following cases in which prefix is empty:
//    --- 6.1.1 ---
//    [Before] if prefix is empty, and in this directory there are multiple
//             files and they do not have a common prefix,
//    cmd> help $sdfgh
//    [After] print all the file names
//    .               ..              Homework_3.docx Homework_3.pdf  Makefile
//    MustExist.txt   MustRemove.txt  bin             dofiles         include
//    lib             mydb            ref             src             testdb
//    cmd> help $sdfgh
//    --- 6.1.2 ---
//    [Before] if prefix is empty, and in this directory there are multiple
//             files and all of them have a common prefix,
//    cmd> help $orld
//    [After]
//    ==> auto insert the common prefix and make a beep sound
//    // e.g. in hw3/ref
//    cmd> help mydb-$orld
//    ==> DO NOT print the matched files
//    ==> If "tab" is pressed again, see 6.2
//    --- 6.1.3 ---
//    [Before] if prefix is empty, and only one file in the current directory
//    cmd> help $ydb
//    [After] print out the single file name followed by a ' '
//    // e.g. in hw3/bin
//    cmd> help mydb $
//    ==> If "tab" is pressed again, make a beep sound and DO NOT re-print
//        the singly-matched file
//    --- 6.2 ---
//    [Before] with a prefix and with mutiple matched files
//    cmd> help M$Donald
//    [After]
//    Makefile        MustExist.txt   MustRemove.txt
//    cmd> help M$Donald
//    --- 6.3 ---
//    [Before] with a prefix and with mutiple matched files,
//             and these matched files have a common prefix
//    cmd> help Mu$k
//    [After]
//    ==> auto insert the common prefix and make a beep sound
//    ==> DO NOT print the matched files
//    cmd> help Must$k
//    --- 6.4 ---
//    [Before] with a prefix and with a singly matched file
//    cmd> help MustE$aa
//    [After] insert the remaining of the matched file name followed by a ' '
//    [TODO] FIXED by Ric @ 10-22-2018
//    ==> and make a beep sound
//    cmd> help MustExist.txt $aa
//    ==> If "tab" is pressed again, make a beep sound and DO NOT re-print
//        the singly-matched file
//    --- 6.5 ---
//    [Before] with a prefix and NO matched file
//    cmd> help Ye$kk
//    [After] beep and stay in the same location
//    cmd> help Ye$kk
//
//    [Note] The counting of tab press is reset after "newline" is entered.
//
// 7. FIRST WORD NO MATCH
//    --- 7.1 ---
//    [Before] Cursor NOT on the first word and NOT matched command
//    cmd> he haha$kk
//    [After Tab]
//    ==> Beep and stay in the same location

void CommandLineInterface::listCmd(const string& str) {
    assert(str.empty() || str[0] != ' ');

    if (str.size()) {
        assert(!str.empty());

        if (size_t firstSpacePos = str.find_first_of(' '); firstSpacePos != string::npos) {  // already has ' '; Cursor NOT on first word
            assert(_tabPressCount != 0);
            CmdExec* e = getCmd(str.substr(0, firstSpacePos));  // first word

            // [case 6] Singly matched on second+ tab
            // [case 7] no match; cursor not on first word
            if (e == nullptr || (_tabPressCount > 1 && !listCmdDir(str))) {
                beep();
                return;
            }

            // [case 5] Singly matched on first tab
            else if (_tabPressCount == 1) {
                cout << endl;
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
 * @brief list the files that match the last word in `cmd`.
 *
 * @param cmd the command line with leading ' ' removed; there must be a ' ' in cmd
 * @return true if printing files
 * @return false if completing (part of) the word
 */
bool CommandLineInterface::listCmdDir(const string& cmd) {
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
        cerr << "Error: unexpected quote stripping result!!" << endl;
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

    searchString = searchString->substr(lastSpacePos + 1, searchString->size() - (lastSpacePos + 1));

    // if the search string ends with a backslash,
    // we will remove it from the search string,
    // but we will flag it to do specialized treatments later
    bool trailingBackslash = false;
    if (searchString->back() == '\\') {
        searchString->pop_back();
        trailingBackslash = true;
    }

    string filename;
    if (!myStrGetTok2(*searchString, filename)) {
        return false;
    }

    auto [dirname, basename] = std::invoke(
        [&filename]() -> std::pair<string, string> {
            if (size_t pos = filename.find_last_of("/"); pos != string::npos) {
                return {filename.substr(0, pos + 1), filename.substr(pos + 1)};
            } else {
                return {"./", filename};
            }
        });

    vector<string> files = listDir(basename, dirname);

    if (trailingBackslash) {
        // clang++ does not support structured binding capture by reference with OpenMP
        std::erase_if(files, [this, &basename = basename](string const& file) { return !isSpecialChar(file[basename.size()]); });
    }

    std::erase_if(files, [](std::string const& file) { return file.starts_with("."); });

    // no matched file
    if (files.size() == 0) {
        return false;
    }

    string autoCompleteStr = files[0].substr(basename.size(), files[0].size() - basename.size());

    // [FIXED] 2018/10/20 by Ric
    // singly matched file or directory
    if (files.size() == 1) {
        if (basename.size() == 0) {  // cmd.back() == ' '
            assert(cmd.back() == ' ' || cmd.back() == '/');

            if (lastSpacePos >= autoCompleteStr.size()) {
                string cmdLast = cmd.substr(lastSpacePos - autoCompleteStr.size() + 1, autoCompleteStr.size());
                if (cmdLast == autoCompleteStr) return false;
            }
        }

        // if outside of a pair of quote, prepend ', " in the file/dir name with backslash
        if (incompleteQuotes.empty()) {
            for (size_t i = 0; i < autoCompleteStr.size(); ++i) {
                if (isSpecialChar(autoCompleteStr[i])) {
                    autoCompleteStr.insert(i, "\\");
                    ++i;
                }
            }
        }

        // if the original string terminates with a backslash, and the string to complete starts with '\',
        // the completion should start from the position of backslash
        // suppose completing a\ b.txt
        // > somecmd a\_[Tab] --> autoCompleteStr = "\ b.txt"
        // > somecmd a\ b.txt <-- should complete like this
        if (trailingBackslash && autoCompleteStr[0] == '\\') {
            cout << '\b';
        }

        std::ranges::for_each(autoCompleteStr, [this](char ch) { insertChar(ch); });

        if (fs::is_directory(dirname + files[0])) {
            insertChar('/');
        } else {
            if (!incompleteQuotes.empty()) insertChar(incompleteQuotes[0]);
            insertChar(' ');
        }

        // autocomplete; do not reprint cmd
        return false;
    }

    bool insertedSomeCharacters = false;

    for (size_t i = basename.size(); i < files[0].size(); ++i) {
        if (any_of(next(files.begin()), files.end(),
                   [&i, files](string const& file) {
                       return i >= file.size() || file[i] != files[0][i];
                   }))
            break;

        insertChar(files[0][i]);
        insertedSomeCharacters = true;
    }

    // [FIX] 2018/10/21 by Ric; don't change line
    if (insertedSomeCharacters) {
        return false;  // multi-matched and auto complete
    }

    // [case 6.2] multiple matched files
    for (auto& file : files) {
        for (size_t i = 0; i < file.size(); ++i) {
            if (isSpecialChar(file[i])) {
                file.insert(i, "\\");
                ++i;
            }
        }
    }

    std::ranges::sort(files, [](std::string const& a, std::string const& b) { return toLowerString(a) < toLowerString(b); });

    for (auto& file : files) {
        namespace TF = TextFormat;
        file = TF::LS_COLOR(file, dirname);
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

    cout << '\n'
         << table.to_string() << endl;
}

// cmd is a copy of the original input
//
// return the corresponding CmdExec* if "cmd" matches any command in _cmdMap
// return 0 if not found.
//
// Please note:
// ------------
// 1. The mandatory part of the command string (stored in _cmdMap) must match
// 2. The optional part can be partially omitted.
// 3. All string comparison are "case-insensitive".
//
CmdExec*
CommandLineInterface::getCmd(string cmd) {
    CmdExec* e = nullptr;

    for (unsigned i = 0; i < cmd.size(); ++i) {
        cmd[i] = toupper(cmd[i]);
        string check = cmd.substr(0, i + 1);
        if (_cmdMap.find(check) != _cmdMap.end())
            e = _cmdMap[check].get();
        if (e != nullptr) {
            string optCheck = cmd.substr(i + 1);
            if (e->checkOptCmd(optCheck))
                return e;  // match found!!
            else
                e = nullptr;
        }
    }
    return e;
}

//----------------------------------------------------------------------
//    Member Function for class CmdExec
//----------------------------------------------------------------------

// Called by "getCmd()"
// Check if "check" is a matched substring of "_optCmd"...
// if not, return false.
//
// Perform case-insensitive checks
//
bool CmdExec::checkOptCmd(const string& check) const {
    if (check.size() > _optCmd.size()) return false;
    for (unsigned i = 0, n = _optCmd.size(); i < n; ++i) {
        if (!check[i]) return true;
        char ch1 = tolower(_optCmd[i]);
        char ch2 = tolower(check[i]);
        if (ch1 != ch2) return false;
    }
    return true;
}

void CommandLineInterface::printPrompt() const {
    cout << _prompt << std::flush;
}

void CommandLineInterface::clearConsole() const {
#ifdef _WIN32
    int result = system("cls");
#else
    int result = system("clear");
#endif
    if (result != 0) {
        cerr << "Error clearing the console!!" << endl;
    }
}
