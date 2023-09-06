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
#include <unordered_map>
#include <utility>
#include <vector>

#include "argparse/argGroup.hpp"
#include "argparse/argparse.hpp"
#include "cli/cliCharDef.hpp"
#include "jthread/jthread.hpp"
#include "util/logger.hpp"

class CommandLineInterface;

//----------------------------------------------------------------------
//    External declaration
//----------------------------------------------------------------------
extern CommandLineInterface cli;
extern dvlab::utils::Logger logger;

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

/**
 * @brief class specification for commands that uses
 *        ArgParse::ArgumentParser to parse and generate help messages.
 *
 */
class Command {
    using ParserDefinition = std::function<void(ArgParse::ArgumentParser&)>;
    using Precondition = std::function<bool()>;
    using OnParseSuccess = std::function<CmdExecResult(ArgParse::ArgumentParser const&)>;

public:
    Command(std::string const& name, ParserDefinition defn, OnParseSuccess on)
        : _parser{name, {.exitOnFailure = false}}, _parserDefinition{defn}, _onParseSuccess{on} {}
    Command(std::string const& name)
        : Command(name, nullptr, nullptr) {}

    bool initialize(size_t numRequiredChars);
    CmdExecResult execute(std::string const& option);
    std::string const& getName() const { return _parser.getName(); }
    size_t getNumRequiredChars() const { return _parser.getNumRequiredChars(); }
    void setNumRequiredChars(size_t numRequiredChars) { _parser.numRequiredChars(numRequiredChars); }
    void printUsage() const { _parser.printUsage(); }
    void printSummary() const { _parser.printSummary(); }
    void printHelp() const { _parser.printHelp(); }

    void addSubCommand(Command const& cmd);

private:
    ParserDefinition _parserDefinition;  // define the parser's arguments and traits
    OnParseSuccess _onParseSuccess;      // define the action to take on parse success
    ArgParse::ArgumentParser _parser;

    void printMissingParserDefinitionErrorMsg() const;
    void printMissingOnParseSuccessErrorMsg() const;
};

//----------------------------------------------------------------------
//    Base class : CmdParser
//----------------------------------------------------------------------

class CommandLineInterface {
    static constexpr size_t READ_BUF_SIZE = 65536;
    static constexpr size_t PG_OFFSET = 10;

    using CmdMap = std::unordered_map<std::string, std::unique_ptr<Command>>;
    using CmdRegPair = std::pair<std::string, std::unique_ptr<Command>>;

public:
    /**
     * @brief Construct a new Command Line Interface object
     *
     * @param prompt the prompt of the CLI
     */
    CommandLineInterface(const std::string& prompt) : _prompt{prompt} {
        _readBuf.reserve(READ_BUF_SIZE);
    }

    bool openDofile(const std::string& filepath);
    void closeDofile();

    bool registerCommand(Command cmd);
    bool registerAlias(const std::string& alias, const std::string& replaceStr);
    bool deleteAlias(const std::string& alias);
    Command* getCommand(std::string const& cmd) const;

    CmdExecResult executeOneLine();

    bool saveVariables(std::string const& filepath, std::span<std::string> arguments);

    void sigintHandler(int signum);
    bool stopRequested() const { return _currCmd.has_value() && _currCmd->get_stop_token().stop_requested(); }

    // printing functions
    void listAllCommands() const;
    void printHistory() const;
    void printHistory(size_t nPrint) const;
    void beep() const;
    void clearTerminal() const;

    struct CLI_ListenConfig {
        bool allowBrowseHistory = true;
        bool allowTabCompletion = true;
    };

    CmdExecResult listenToInput(std::istream& istr, std::string const& prompt, CLI_ListenConfig const& config = {true, true});
    std::string const& getReadBuf() const { return _readBuf; }

private:
    // Private member functions
    void resetBuffer();
    void printPrompt() const;

    int getChar(std::istream&) const;
    CmdExecResult readOneLine(std::istream&);
    std::pair<Command*, std::string> parseOneCommandFromQueue();

    enum TabActionResult {
        AUTOCOMPLETE,
        LIST_OPTIONS,
        NO_OP
    };
    // tab-related features features
    void onTabPressed();
    // onTabPressed subroutines
    TabActionResult matchCommandsAndAliases(std::string const& str);
    TabActionResult matchVariables(std::string const& str);
    TabActionResult matchFiles(std::string const& str);

    // helper functions
    std::vector<std::string> getFileMatches(std::filesystem::path const& filepath) const;
    bool autocomplete(std::string prefixCopy, std::vector<std::string> const& strs, bool inQuotes);
    void printAsTable(std::vector<std::string> words) const;

    // Helper functions
    bool moveCursorTo(size_t pos);
    bool deleteChar();
    void insertChar(char);
    void deleteLine();
    void reprintCommand();
    void moveToHistory(size_t index);
    bool addUserInputToHistory();
    void retrieveHistory();

    std::string replaceVariableKeysWithValues(std::string const& str) const;

    inline bool isSpecialChar(char ch) const { return _specialChars.find_first_of(ch) != std::string::npos; }

    static std::string const _specialChars;  // The characters that are identified as special characters when parsing
    // Data members
    std::string _prompt;         // command prompt
    std::string _readBuf;        // read buffer
    size_t _cursorPosition = 0;  // current cursor position on the readBuf. This variable is signed

    std::vector<std::string> _history;       // oldest:_history[0],latest:_hist.back()
    size_t _historyIdx = 0;                  // (1) Position to insert history string
                                             //     i.e. _historyIdx = _history.size()
                                             // (2) When up/down/pgUp/pgDn is pressed,
                                             //     position to history to retrieve
    size_t _tabPressCount = 0;               // The number of tab pressed
    bool _listeningForInputs = false;        // whether the CLI is listening for inputs
    bool _tempCmdStored = false;             // When up/pgUp is pressed, current line
                                             // will be stored in _history and
                                             // _tempCmdStored will be true.
                                             // Reset to false when new command added
    CmdMap _commands;                        // map from string to command
    std::stack<std::ifstream> _dofileStack;  // For recursive dofile calling
    std::queue<std::string> _commandQueue;
    std::optional<jthread::jthread> _currCmd = std::nullopt;  // the current (ongoing) command
    std::unordered_map<std::string, std::string> _variables;  // stores the variables key-value pairs, e.g., $1, $INPUT_FILE, etc...
    dvlab::utils::Trie _identifiers;
    std::unordered_map<std::string, std::string> _aliases;
};
