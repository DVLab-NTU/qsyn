/****************************************************************************
  FileName     [ cmdParser.cpp ]
  PackageName  [ cmd ]
  Synopsis     [ Define command parsing member functions for class CmdParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "cmdParser.h"

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <cstdlib>     // for exit
#include <filesystem>  // lines 12-12
#include <fstream>
#include <iostream>  // for cin, cout

#include "util.h"

using namespace std;
namespace fs = std::filesystem;

//----------------------------------------------------------------------
//    External funcitons
//----------------------------------------------------------------------
void mybeep();

//----------------------------------------------------------------------
//    Member Function for class cmdParser
//----------------------------------------------------------------------
// return false if file cannot be opened
// Please refer to the comments in "DofileCmd::exec", cmdCommon.cpp
bool CmdParser::openDofile(const string& dof) {
    // TODO...
    if (_dofile != 0)
        if (!pushDofile()) return false;
    // Keep this line for TODO's
    _dofile = new ifstream(dof.c_str());
    if (!_dofile->is_open()) {
        closeDofile();
        return false;
    }
    return true;
}

// Must make sure _dofile != 0
void CmdParser::closeDofile() {
    assert(_dofile != 0);
    // TODO...
    // Keep this line for TODO's
    delete _dofile;
    _dofile = 0;
    popDofile();
}

// Removed for TODO's
// return false if stack overflow
bool CmdParser::pushDofile() {
#ifdef __APPLE__
#define DOFILE_STACK_LIMIT 252
#else
#define DOFILE_STACK_LIMIT 1024
#endif
    if (_dofileStack.size() >= DOFILE_STACK_LIMIT) {
        cerr << "Error: dofile stack overflow (" << DOFILE_STACK_LIMIT
             << ")" << endl;
        return false;
    }
    _dofileStack.push(_dofile);
    _dofile = 0;
    return true;
}

// Removed for TODO's
// return false if stack empty
bool CmdParser::popDofile() {
    if (_dofileStack.empty())
        return false;
    _dofile = _dofileStack.top();
    _dofileStack.pop();
    return true;
}

// Return false if registration fails
bool CmdParser::regCmd(const string& cmd, unsigned nCmp, unique_ptr<CmdExec>&& e) {
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

// Return false on "quit" or if exception happens
CmdExecStatus
CmdParser::execOneCmd() {
    bool newCmd = false;
    if (_dofile != 0)
        newCmd = readCmd(*_dofile);
    else
        newCmd = readCmd(cin);

    // execute the command
    if (newCmd) {
        string option;
        CmdExec* e = parseCmd(option);
        if (e != 0)
            return e->exec(option);
    }
    return CMD_EXEC_NOP;
}

// For each CmdExec* in _cmdMap, call its "help()" to print out the help msg.
// Print an endl at the end.
void CmdParser::printHelps() const {
    // TODO...
    for (const auto& mi : _cmdMap)
        mi.second->summary();

    cout << endl;
}

void CmdParser::printHistory() const {
    printHistory(_history.size());
}

void CmdParser::printHistory(size_t nPrint) const {
    assert(_tempCmdStored == false);
    if (_history.empty()) {
        cout << "Empty command history!!" << endl;
        return;
    }
    size_t s = _history.size();
    for (auto i = s - min(s, nPrint); i < s; ++i)
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
CmdExec*
CmdParser::parseCmd(string& option) {
    assert(_tempCmdStored == false);
    assert(!_history.empty());
    string str = _history.back();

    // TODO...
    assert(str[0] != 0 && str[0] != ' ');
    size_t n = str.find(' ');
    string cmd = str.substr(0, n);
    CmdExec* e = getCmd(cmd);
    if (!e) {
        cerr << "Illegal command!! (" << cmd << ")" << endl;
    } else if (n != string::npos) {
        size_t opt = str.find_first_not_of(' ', n);
        if (opt != string::npos)
            option = str.substr(opt);
    }
    return e;
}

// Remove this function for TODO...
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

void CmdParser::listCmd(const string& str) {
    // TODO...
    CmdMap::const_iterator bi, ei, ti;
    size_t b = str.find_first_not_of(' ');
    string cmd;
    if (b == string::npos) {  // [case 1] empty string; list all
        bi = _cmdMap.begin();
        ei = _cmdMap.end();
    } else {
        cmd = str.substr(b);  // remove leading ' '
        assert(!cmd.empty());
        size_t bn = cmd.find_first_of(' ');
        if (bn != string::npos) {                    // already has ' '; Cursor NOT on first word
            CmdExec* e = getCmd(cmd.substr(0, bn));  // first word
            if (e != 0) {
                assert(_tabPressCount != 0);
                if (_tabPressCount == 1) {
                    // [case 5] Singly matched on first tab
                    cout << endl;
                    e->usage();
                }
                // [case 6] Singly matched on second+ tab
                else if (!listCmdDir(cmd)) {
                    mybeep();
                    return;
                }
                reprintCmd();
            } else
                mybeep();  // [case 7] no match; cursor not on first word
            return;        // from cases 5, 6, 7
        }

        // cursor on first word
        for (size_t i = 0, n = cmd.size(); i < n; ++i)
            cmd[i] = toupper(cmd[i]);

        if (getCmd(cmd)) {  // cmd is enough to determine a single cmd
            bi = _cmdMap.find(cmd);
            if (bi == _cmdMap.end()) {
                bi = _cmdMap.lower_bound(cmd);
                --bi;
            }
            ei = bi;
            ++ei;
        } else {
            string cmdN = cmd;
            cmdN[cmdN.size() - 1] = cmd[cmd.size() - 1] + 1;
            bi = _cmdMap.lower_bound(cmd);
            ei = _cmdMap.lower_bound(cmdN);
            if (bi == ei) {
                mybeep();
                return;
            }  // [case 4] no match in 1st wd
        }
    }  // end of cmd string processing
    // cases 1, 2, 3 go here
    ti = bi;
    ++ti;
    if (ti == ei) {  // [case 3] single command; insert ' '
        string ss = bi->first + bi->second->getOptCmd();
        for (size_t i = cmd.size(), n = ss.size(); i < n; ++i)
            insertChar(ss[i]);
        insertChar(' ');
    } else {  // [case 2] multiple matches
        size_t cmdSpacing = 0;
        size_t cmdsPerLine = 5;
        for (auto itr = bi; itr != ei; ++itr) {
            string ss = itr->first + itr->second->getOptCmd();
            if (ss.size() > cmdSpacing) {
                cmdSpacing = ss.size();
            }
        }
        while (cmdsPerLine > 1 && cmdsPerLine * (cmdSpacing + 2) > 60) {
            cmdsPerLine--;
        }
        cmdSpacing = 60 / cmdsPerLine;
        size_t count = 0;
        for (auto itr = bi; itr != ei; ++itr) {
            if ((count++ % cmdsPerLine) == 0) cout << endl;
            string ss = itr->first + itr->second->getOptCmd();
            cout << setw(cmdSpacing) << left << ss;
        }
        reprintCmd();
    }
    _tabPressCount = 0;
}

// Remove this function for TODO...
// cmd is the command line with leading ' ' removed
// There must be a ' ' in cmd
//
// Called by listCmd() when the first word of cmd has a singly matched
// command and usage has been printed.
//
// This function is to list the files that match the last word in cmd
//
// return false if list of files is NOT printed.
// (i.e. Stay in the same line. Don't call reprintCmd() in listCmd())
bool CmdParser::listCmdDir(const string& cmd) {
    assert(cmd[0] != ' ');
    string tmp;
    string incompleteQuotes;
    bool trailingBackslash = false;
    if (stripQuotes(cmd, tmp)) {
        incompleteQuotes = "";
    } else if (stripQuotes(cmd + "\"", tmp)) {
        incompleteQuotes = "\"";
    } else if (stripQuotes(cmd + "\'", tmp)) {
        incompleteQuotes = "\'";
    } else {
        cerr << "Error: unexpected quote stripping result!!" << endl;
        return false;
    }
    // cerr << "tmp = \"" << tmp << "\"" << endl;
    auto findLastSpace = [](const string& str) -> size_t {
        size_t pos = string::npos;
        pos = str.find_last_of(" ");
        while (pos != string::npos && str[pos - 1] == '\\') {
            pos = str.find_last_of(" ", pos - 2);
        }
        return pos;
    };

    // auto countNumSpecialChars = [](const string& str, size_t pos = 0) -> size_t {
    //     size_t numSpecialChars = 0;
    //     for (size_t i = pos; i < str.size(); ++i) {
    //         if (str[i] == '\'' || str[i] == '\"' || str[i] == ' ') {
    //             numSpecialChars++;
    //         }
    //     }
    //     return numSpecialChars;
    // };

    size_t bn = findLastSpace(tmp);
    string promptname, filename, dirname, basename;
    vector<string> files;
    assert(bn != string::npos);       // must have ' '
    size_t fLen = tmp.size() - ++bn;  // promptname length
    // size_t numSpecial, baseNumSpecial;
    // If (cmd.back() == ' ') ==> fLen = 0
    if (fLen != 0) {
        promptname = tmp.substr(bn, fLen);

        // cerr << "Prompt name \"" << promptname << "\"" << endl;
        if (promptname.back() == '\\') {
            promptname.pop_back();
            trailingBackslash = true;
        }

        size_t pos = promptname.find_last_of("/");
        if (pos == string::npos) pos = 0;

        if (!myStrGetTok2(promptname, filename)) {
            return false;
        }
        // cerr << "File name   \"" << filename   << "\"" << endl;
        pos = filename.find_last_of("/");
        if (pos != string::npos) {
            dirname = filename.substr(0, pos);
            basename = filename.substr(pos + 1);
        } else {
            dirname = "";
            basename = filename;
        }
        fLen = basename.size();
    }

    listDir(files, basename, "./" + dirname);
    if (files.size() == 0) {  // [case 6.5] no matched file
        return false;
    }
    string ff = files[0].substr(fLen, files[0].size() - fLen);
    if (files.size() == 1) {  // [case 6.1.3 & 6.4] singly matched file
        // [FIX] 2018/10/20 by Ric for 6.1.3 and 6.4
        // Check if the last part of cmd is ff followed by ' '
        // If yes, DO NOT re-print the last part of the cmd
        if (fLen == 0) {  // cmd.back() == ' '
            assert(cmd.back() == ' ' || cmd.back() == '/');
            size_t en = cmd.find_last_not_of(' ');
            if (en >= ff.size()) {
                string cmdLast = cmd.substr(en - ff.size() + 1, ff.size());
                if (cmdLast == ff) return false;
            }
        }
        if (incompleteQuotes.empty()) {
            for (size_t i = 0; i < ff.size(); ++i) {
                switch (ff[i]) {
                    case ' ':
                    case '\"':
                    case '\'':
                        ff.insert(i, "\\");
                        ++i;
                        break;
                    default:
                        break;
                }
            }
        }
        if (trailingBackslash && ff[0] == '\\') {
            ff = ff.substr(1);
        }
        for (size_t i = 0, n = ff.size(); i < n; ++i) {
            insertChar(ff[i]);
        }
        // cerr << ("./" + dirname + "/" + files[0]) << " ";
        if (fs::is_directory("./" + dirname + "/" + files[0])) {
            // cerr << "is a dir!!" << endl;
            insertChar('/');
        } else {
            // cerr << "is a file!!" << endl;
            if (!incompleteQuotes.empty()) insertChar(incompleteQuotes[0]);
            insertChar(' ');
        }
        // [FIX] 2018/10/20 by Ric for 6.1.3 and 6.4
        // DO NOT re-print cmd
        return false;
    } else {
        bool doCheck = true, doneInsert = false;
        for (size_t i = fLen, n = files[0].size(); i < n; ++i) {
            for (size_t j = 1, m = files.size(); j < m; ++j)
                if ((i >= files[j].size()) ||
                    files[j][i] != files[0][i]) {
                    doCheck = false;
                    break;
                }
            if (!doCheck) break;
            insertChar(files[0][i]);
            doneInsert = true;
        }
        if (!doneInsert) {  // [case 6.2] multiple matched files
            size_t fileSpacing = 0;
            size_t filesPerLine = 5;
            for (auto& file : files) {
                for (size_t i = 0; i < file.size(); ++i) {
                    switch (file[i]) {
                        case ' ':
                        case '\"':
                        case '\'':
                            file.insert(i, "\\");
                            ++i;
                            break;
                        default:
                            break;
                    }
                }
                if (file.size() > fileSpacing) {
                    fileSpacing = file.size();
                }
            }
            while (filesPerLine > 1 && filesPerLine * (fileSpacing + 2) > 80) {
                filesPerLine--;
            }
            fileSpacing = 80 / filesPerLine;
            size_t count = 0;
            for (const auto& file : files) {
                if (count++ % filesPerLine == 0) cout << endl;
                cout << setw(fileSpacing) << left << file;
            }
        }
        // [FIX] 2018/10/21 by Ric; don't change line
        else
            return false;  // [case 6.3] multi-matched and auto complete
    }
    return true;
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
CmdParser::getCmd(string cmd) {
    CmdExec* e = 0;
    // TODO...
    // call CmdExec::checkOptCmd(const string&) if needed...
    for (unsigned i = 0, n = cmd.size(); i < n; ++i) {
        cmd[i] = toupper(cmd[i]);
        string check = cmd.substr(0, i + 1);
        if (_cmdMap.find(check) != _cmdMap.end())
            e = _cmdMap[check].get();
        if (e != 0) {
            string optCheck = cmd.substr(i + 1);
            if (e->checkOptCmd(optCheck))
                return e;  // match found!!
            else
                e = 0;
        }
    }
    return e;
}

//----------------------------------------------------------------------
//    Member Function for class CmdExec
//----------------------------------------------------------------------
// return false if option contains an token
bool CmdExec::lexNoOption(const string& option) const {
    string err;
    myStrGetTok2(option, err);
    if (err.size()) {
        errorOption(CMD_OPT_EXTRA, err);
        return false;
    }
    return true;
}

// Return false if error options found
// "optional" = true if the option is optional XD
// "optional": default = true
//
bool CmdExec::lexSingleOption(const string& option, string& token, bool optional) const {
    string stripped;
    if (!stripQuotes(option, stripped)) {
        cerr << "[Error] Missing ending quote!!!!" << endl;
        return false;
    }
    size_t n = myStrGetTok2(stripped, token);
    if (!optional) {
        if (token.size() == 0) {
            errorOption(CMD_OPT_MISSING, "");
            return false;
        }
    }
    if (n != string::npos) {
        errorOption(CMD_OPT_EXTRA, option.substr(n));
        return false;
    }
    return true;
}

// if nOpts is specified (!= 0), the number of tokens must be exactly = nOpts
// Otherwise, return false.
//
bool CmdExec::lexOptions(const string& option, vector<string>& tokens, size_t nOpts) const {
    string token, stripped;
    if (!stripQuotes(option, stripped)) {
        cerr << "[Error] Missing ending quote!!!!" << endl;
        return false;
    }
    size_t n = myStrGetTok2(stripped, token);
    while (token.size()) {
        tokens.push_back(token);
        n = myStrGetTok2(stripped, token, n);
    }
    if (nOpts != 0) {
        if (tokens.size() < nOpts) {
            errorOption(CMD_OPT_MISSING, "");
            return false;
        }
        if (tokens.size() > nOpts) {
            errorOption(CMD_OPT_EXTRA, tokens[nOpts]);
            return false;
        }
    }
    return true;
}

CmdExecStatus
CmdExec::errorOption(CmdOptionError err, const string& opt) {
    switch (err) {
        case CMD_OPT_MISSING:
            cerr << "Error: Missing option";
            if (opt.size()) cerr << " after (" << opt << ")";
            cerr << "!!" << endl;
            break;
        case CMD_OPT_EXTRA:
            cerr << "Error: Extra option!! (" << opt << ")" << endl;
            break;
        case CMD_OPT_ILLEGAL:
            cerr << "Error: Illegal option!! (" << opt << ")" << endl;
            break;
        case CMD_OPT_FOPEN_FAIL:
            cerr << "Error: cannot open file \"" << opt << "\"!!" << endl;
            break;
        default:
            cerr << "Error: Unknown option error type!! (" << err << ")" << endl;
            exit(-1);
    }
    return CMD_EXEC_ERROR;
}

// Remove this function for TODO...
//
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

void CmdParser::printPrompt() const {
    cout << _prompt;
}
