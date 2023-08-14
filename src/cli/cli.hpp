/****************************************************************************
  FileName     [ cli.hpp ]
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
#include <utility>
#include <vector>

#include "argparse/argparse.hpp"
#include "cli/cliCharDef.hpp"
#include "jthread/jthread.hpp"
#include "util/logger.hpp"

class CommandLineInterface;

//----------------------------------------------------------------------
//    External declaration
//----------------------------------------------------------------------
extern CommandLineInterface cli;
extern dvlab_utils::Logger logger;

//----------------------------------------------------------------------
//    command execution status
//----------------------------------------------------------------------
enum class CmdExecResult {
    DONE = 0,
    ERROR = 1,
    QUIT = 2,
    NOP = 3,
    INTERRUPTED = 4,
};

//----------------------------------------------------------------------
//    Base class : CmdExec
//----------------------------------------------------------------------

class CmdExec {
public:
    CmdExec() {}
    virtual ~CmdExec() {}

    virtual bool initialize() = 0;
    virtual CmdExecResult exec(const std::string&) = 0;
    virtual void usage() const = 0;
    virtual void summary() const = 0;
    virtual void help() const = 0;

    void setOptCmd(const std::string& str) { _optCmd = str; }
    bool checkOptCmd(const std::string& check) const;
    const std::string& getOptCmd() const { return _optCmd; }

private:
    std::string _optCmd;
};

/**
 * @brief class specification for commands that uses
 *        ArgParse::ArgumentParser to parse and generate help messages.
 *
 */
class ArgParseCmdType : public CmdExec {
    using ParserDefinition = std::function<void(ArgParse::ArgumentParser&)>;
    using Precondition = std::function<bool()>;
    using OnParseSuccess = std::function<CmdExecResult(ArgParse::ArgumentParser const&)>;

public:
    ArgParseCmdType(std::string const& name) { _parser.name(name); }
    ~ArgParseCmdType() {}

    bool initialize() override;
    CmdExecResult exec(std::string const& option) override;
    void usage() const override { _parser.printUsage(); }
    void summary() const override { _parser.printSummary(); }
    void help() const override { _parser.printHelp(); }

    ParserDefinition parserDefinition;  // define the parser's arguments and traits
    Precondition precondition;          // define the parsing precondition

    OnParseSuccess onParseSuccess;  // define the action to take on parse success

private:
    ArgParse::ArgumentParser _parser;

    void printMissingParserDefinitionErrorMsg() const;
    void printMissingOnParseSuccessErrorMsg() const;
};

//----------------------------------------------------------------------
//    Base class : CmdParser
//----------------------------------------------------------------------

class CommandLineInterface {
    static const size_t READ_BUF_SIZE = 65536;
    static const size_t PG_OFFSET = 10;

    using CmdMap = std::map<const std::string, std::unique_ptr<CmdExec>>;
    using CmdRegPair = std::pair<const std::string, std::unique_ptr<CmdExec>>;

public:
    /**
     * @brief Construct a new Command Line Interface object
     *
     * @param prompt the prompt of the CLI
     */
    CommandLineInterface(const std::string& prompt) : _prompt{prompt} {
        _readBuf.reserve(READ_BUF_SIZE);
    }
    virtual ~CommandLineInterface() {}

    bool openDofile(const std::string& dof);
    void closeDofile();

    bool regCmd(const std::string&, unsigned, std::unique_ptr<CmdExec>&&);
    CmdExecResult executeOneLine();

    void addArgument(std::string const& val) {
        _arguments.emplace_back(val);
        _variables.emplace(std::to_string(_arguments.size()), val);
    }

    CmdExec* getCmd(std::string);

    void sigintHandler(int signum);
    bool stop_requested() const { return _currCmd.has_value() && _currCmd->get_stop_token().stop_requested(); }

    // printing functions
    void printHelps() const;
    void printHistory() const;
    void printHistory(size_t nPrint) const;
    void beep() const;
    void clearConsole() const;

private:
    // Private member functions
    void printPrompt() const;
    void resetBufAndPrintPrompt();

    int getChar(std::istream&) const;
    bool readCmd(std::istream&);
    std::pair<CmdExec*, std::string> parseOneCommandFromQueue();

    // tab-related features features
    void matchAndComplete(const std::string&);
    // helper functions
    std::pair<CmdMap::const_iterator, CmdMap::const_iterator> getCmdMatches(std::string const& str);
    bool matchFilesAndComplete(const std::string&);
    std::vector<std::string> getMatchedFiles(std::filesystem::path const& filepath) const;
    bool completeCommonChars(std::string prefixCopy, std::vector<std::string> const& strs, bool inQuotes);
    void printAsTable(std::vector<std::string> words) const;

    // Helper functions
    bool moveCursor(int);
    bool deleteChar();
    void insertChar(char);
    void deleteLine();
    void reprintCmd();
    void moveToHistory(int index);
    bool addUserInputToHistory();
    void retrieveHistory();

    std::string replaceVariableKeysWithValues(std::string const& str) const;
    void saveArgumentsInVariables(std::string const& str);

    inline bool isSpecialChar(char ch) const { return _specialChars.find_first_of(ch) != std::string::npos; }

    void askForUserInput(std::istream& istr);

    // Data members
    std::string const _prompt;                   // command prompt
    std::string const _specialChars = "\"\' ;";  // The characters that are identified as special characters when parsing
    std::string _readBuf;                        // read buffer
    size_t _cursorPosition = 0;                  // current cursor postion on the readBuf
    std::vector<std::string> _history;           // oldest:_history[0],latest:_hist.back()
    int _historyIdx = 0;                         // (1) Position to insert history string
                                                 //     i.e. _historyIdx = _history.size()
                                                 // (2) When up/down/pgUp/pgDn is pressed,
                                                 //     position to history to retrieve
    size_t _tabPressCount = 0;                   // The number of tab pressed
    bool _tempCmdStored = false;                 // When up/pgUp is pressed, current line
                                                 // will be stored in _history and
                                                 // _tempCmdStored will be true.
                                                 // Reset to false when new command added
    CmdMap _cmdMap;                              // map from string to command
    std::stack<std::ifstream> _dofileStack;      // For recursive dofile calling
    std::queue<std::string> _commandQueue;
    std::optional<jthread::jthread> _currCmd = std::nullopt;  // the current (ongoing) command
    std::unordered_map<std::string, std::string> _variables;  // stores the variables key-value pairs, e.g., $1, $INPUT_FILE, etc...
    std::vector<std::string> _arguments;                      // stores the extra dofile arguments given when invoking the program
};
