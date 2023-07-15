/****************************************************************************
  FileName     [ argparse.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef AP_ARGPARSE_ARGPARSER_H
#define AP_ARGPARSE_ARGPARSER_H

#include <any>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "myConcepts.h"
#include "myTrie.h"
#include "ordered_hashmap.h"
#include "ordered_hashset.h"
#include "tabler.h"
#include "util.h"

namespace ArgParse {

class Argument;
class ArgumentParser;
class ArgumentGroup;
class SubParsers;

/**
 * @brief Pretty printer for the command usages and helps.
 *
 */
struct Formatter {
public:
    static void printUsage(ArgumentParser parser);
    static void printSummary(ArgumentParser parser);
    static void printHelp(ArgumentParser parser);

    static std::string styledArgName(ArgumentParser parser, Argument const& arg);
    static std::string styledCmdName(std::string const& name, size_t numRequired);

    static std::string getSyntaxString(ArgumentParser parser, Argument const& arg);
    static std::string getSyntaxString(SubParsers parsers);

    static std::string requiredArgBracket(std::string const& str);
    static std::string optionalArgBracket(std::string const& str);

    static void printHelpString(ArgumentParser parser, Argument const& arg);
    static void printHelpString(ArgumentParser parser, SubParsers parsers);
};

using ActionCallbackType = std::function<bool()>;                                 // perform an action and return if it succeeds
using ErrorCallbackType = std::function<void()>;                                  // function to call when some action fails
using ConstraintCallbackType = std::pair<ActionCallbackType, ErrorCallbackType>;  // constraints are defined by an ActionCallbackType that
                                                                                  // returns true if the constraint is met, and an
                                                                                  // ErrorCallbackType that prints the error message if it does not.
struct DummyArgumentType {
    friend std::ostream& operator<<(std::ostream& os, DummyArgumentType const& val) { return os << "dummy"; }
};

template <typename T>
requires Arithmetic<T>
std::string typeString(T);  // explicitly instantiated in apType.cpp
std::string typeString(std::string const&);
std::string typeString(bool);
std::string typeString(DummyArgumentType);

template <typename T>
std::ostream& print(std::ostream& os, T const& val) { return os << val; }

template <typename T>
requires Arithmetic<T>
bool parseFromString(T& val, std::string const& token) { return myStr2Number<T>(token, val); }
bool parseFromString(std::string& val, std::string const& token);
bool parseFromString(bool& val, std::string const& token);
bool parseFromString(DummyArgumentType& val, std::string const& token);

template <typename T>
concept ValidArgumentType = requires(T t) {
    { typeString(t) } -> std::same_as<std::string>;
    { print(std::cout, t) } -> std::same_as<std::ostream&>;
    { parseFromString(t, std::string{}) } -> std::same_as<bool>;
};

template <typename T>
concept IsContainerType = requires(T t) {
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    { t.begin() } -> std::same_as<typename T::const_iterator>;
    { t.end() } -> std::same_as<typename T::const_iterator>;
    { t.size() } -> std::same_as<typename T::size_type>;
    requires !std::same_as<T, std::string>;
    requires !std::same_as<T, std::string_view>;
};

template <typename T>
requires ValidArgumentType<T>
class ArgType {
public:
    using ActionType = std::function<ActionCallbackType(ArgType<T>&)>;
    using ErrorType = std::function<ErrorCallbackType(ArgType<T> const&)>;
    using ConstraintType = std::pair<ActionType, ErrorType>;

    ArgType(T const& val)
        : _value{val}, _name{}, _help{}, _defaultValue{std::nullopt},
          _constValue{std::nullopt}, _actionCallback{}, _metavar{},
          _nargs{1, 1}, _required{false}, _append{false} {}

    friend std::ostream& operator<<(std::ostream& os, ArgType<T> const& arg) {
        if (arg._nargs.upper <= 1) return print(os, arg._value.front());
        return print(os, arg._value);
    }

    // argument decorators

    ArgType& name(std::string const& name);
    ArgType& help(std::string const& help);
    ArgType& required(bool isReq);
    ArgType& defaultValue(T const& val);
    ArgType& action(ActionType const& action);
    ArgType& constValue(T const& val);
    ArgType& metavar(std::string const& metavar);
    ArgType& constraint(ConstraintType const& constraint_error);
    ArgType& constraint(ActionType const& constraint, ErrorType const& onerror = nullptr);
    ArgType& choices(std::initializer_list<T> const& choices);
    ArgType& nargs(size_t n);
    ArgType& nargs(size_t l, size_t u);
    ArgType& nargs(char ch);

    inline bool parse(std::string const& token);
    inline void reset();

    // getters
    // NOTE - only giving the first argument in ArgType<T>
    //      - might need to revisit later
    template <typename Ret>
    Ret get() const {
        if constexpr (IsContainerType<Ret>) {
            return _value;
        } else {
            return _value.front();
        }
    }

    inline T const& getValue() const { return _value.front(); }

    void setValueToConst();

private:
    friend class Argument;
    friend class ArgumentGroup;
    std::vector<T> _value;
    std::string _name;
    std::string _help;
    std::optional<T> _defaultValue;
    std::optional<T> _constValue;
    ActionCallbackType _actionCallback;
    std::string _metavar;
    std::vector<ConstraintCallbackType> _constraintCallbacks;
    struct {
        size_t lower;
        size_t upper;
    } _nargs;

    bool _required : 1;
    bool _append : 1;
};

class Argument {
public:
    Argument()
        : _pimpl{std::make_unique<Model<ArgType<DummyArgumentType>>>(DummyArgumentType{})} {}

    template <typename T>
    Argument(T const& val)
        : _pimpl{std::make_unique<Model<ArgType<T>>>(std::move(val))}, _parsed{false}, _numRequiredChars{1} {}

    ~Argument() = default;

    Argument(Argument const& other)
        : _pimpl(other._pimpl->clone()), _parsed{other._parsed}, _numRequiredChars{other._numRequiredChars} {}

    Argument& operator=(Argument copy) noexcept {
        copy.swap(*this);
        return *this;
    }
    Argument(Argument&& other) noexcept = default;

    void swap(Argument& rhs) noexcept;
    friend void swap(Argument& lhs, Argument& rhs) noexcept;

    friend std::ostream& operator<<(std::ostream& os, Argument const& arg) {
        return arg._pimpl->do_print(os);
    }

    template <typename T>
    requires ValidArgumentType<T>
    operator T() const { return get<T>(); }

    template <typename T>
    requires ValidArgumentType<T>
    T get() const;

    std::string getTypeString() const { return _pimpl->do_getTypeString(); }
    std::string const& getName() const { return _pimpl->do_getName(); }
    std::string const& getHelp() const { return _pimpl->do_getHelp(); }
    size_t getNumRequiredChars() const { return _numRequiredChars; }
    std::string const& getMetavar() const { return _pimpl->do_getMetaVar(); }
    std::vector<ConstraintCallbackType> const& getConstraints() const { return _pimpl->do_getConstraints(); }

    // attributes

    bool hasDefaultValue() const { return _pimpl->do_hasDefaultValue(); }
    bool hasAction() const { return _pimpl->do_hasAction(); }
    bool isRequired() const { return _pimpl->do_isRequired(); }
    bool isParsed() const { return _parsed; }

    // setters

    void setNumRequiredChars(size_t n) { _numRequiredChars = n; }

    // print functions

    void printStatus() const;
    void printDefaultValue(std::ostream& os) const { _pimpl->do_printDefaultValue(os); }

    // action

    void reset();
    bool parse(std::string const& token);

private:
    friend class ArgumentParser;

    struct Concept {
        virtual ~Concept() {}

        virtual std::unique_ptr<Concept> clone() const = 0;

        virtual std::string do_getTypeString() const = 0;
        virtual std::string const& do_getName() const = 0;
        virtual std::string const& do_getHelp() const = 0;
        virtual std::string const& do_getMetaVar() const = 0;
        virtual std::vector<ConstraintCallbackType> const& do_getConstraints() const = 0;

        virtual bool do_hasDefaultValue() const = 0;
        virtual bool do_hasAction() const = 0;
        virtual bool do_isRequired() const = 0;

        virtual std::ostream& do_print(std::ostream& os) const = 0;
        virtual std::ostream& do_printDefaultValue(std::ostream& os) const = 0;

        virtual bool do_parse(std::string const& token) = 0;
        virtual void do_reset() = 0;
    };

    template <typename ArgT>
    struct Model final : Concept {
        ArgT inner;

        Model(ArgT val)
            : inner(std::move(val)) {}
        ~Model() {}

        inline std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        inline std::string do_getTypeString() const override { return typeString(inner._value.front()); }
        inline std::string const& do_getName() const override { return inner._name; }
        inline std::string const& do_getHelp() const override { return inner._help; }
        inline std::string const& do_getMetaVar() const override { return inner._metavar; }
        inline std::vector<ConstraintCallbackType> const& do_getConstraints() const override { return inner._constraintCallbacks; }

        inline bool do_hasDefaultValue() const override { return inner._defaultValue.has_value(); }
        inline bool do_hasAction() const override { return inner._actionCallback != nullptr; }
        inline bool do_isRequired() const override { return inner._required; };

        inline std::ostream& do_print(std::ostream& os) const override { return os << inner; }
        inline std::ostream& do_printDefaultValue(std::ostream& os) const override { return (inner._defaultValue.has_value() ? os << inner._defaultValue.value() : os << "(none)"); }

        inline bool do_parse(std::string const& token) override { return inner.parse(token); }
        inline void do_reset() override { inner.reset(); }
    };

    std::unique_ptr<Concept> _pimpl;

    bool _parsed;
    size_t _numRequiredChars;
};

/**
 * @brief A view for adding argument groups.
 *        All copies of this class represents the same underlying group.
 *
 */
class ArgumentGroup {
    struct ArgumentGroupImpl {
        ArgumentGroupImpl(ArgumentParser& parser)
            : _parser{parser} {}
        ArgumentParser& _parser;
        ordered_hashset<std::string> _arguments;
        bool _required;
        bool _parsed;
    };

public:
    ArgumentGroup(ArgumentParser& parser)
        : _pimpl{std::make_shared<ArgumentGroupImpl>(parser)} {}

    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addArgument(std::string const& name);

    bool contains(std::string const& name) const { return _pimpl->_arguments.contains(name); }
    ArgumentGroup required(bool isReq) {
        _pimpl->_required = isReq;
        return *this;
    }
    void setParsed(bool isParsed) { _pimpl->_parsed = isParsed; }

    bool isRequired() const { return _pimpl->_required; }
    bool isParsed() const { return _pimpl->_parsed; }

    size_t size() const noexcept { return _pimpl->_arguments.size(); }

    ordered_hashset<std::string> const& getArguments() const { return _pimpl->_arguments; }

private:
    std::shared_ptr<ArgumentGroupImpl> _pimpl;
};

/**
 * @brief A view for adding subparsers.
 *        All copies of this class represents the same underlying group of subparsers.
 *
 */
class SubParsers {
private:
    struct SubParsersImpl {
        ordered_hashmap<std::string, ArgumentParser> subparsers;
        std::string activatedSubparser;
        std::string help;
        bool required;
        bool parsed;
    };
    std::shared_ptr<SubParsersImpl> _pimpl;

public:
    SubParsers() : _pimpl{std::make_shared<SubParsersImpl>()} {}
    void setParsed(bool isParsed) { _pimpl->parsed = isParsed; }
    SubParsers required(bool isReq) {
        _pimpl->required = isReq;
        return *this;
    }
    SubParsers help(std::string const& help) {
        _pimpl->help = help;
        return *this;
    }

    ArgumentParser addParser(std::string const& n);

    size_t size() const noexcept { return _pimpl->subparsers.size(); }

    auto const& getSubParsers() const { return _pimpl->subparsers; }
    auto const& getHelp() const { return _pimpl->help; }

    bool isRequired() const { return _pimpl->required; }
    bool isParsed() const { return _pimpl->parsed; }
};

/**
 * @brief A view for argument parsers.
 *        All copies of this class represents the same underlying parsers.
 *
 */
class ArgumentParser {
    friend class Formatter;

public:
    ArgumentParser() : _pimpl{std::make_shared<ArgumentParserImpl>()} {}
    ArgumentParser(std::string const& n) : _pimpl{std::make_shared<ArgumentParserImpl>()} {
        this->name(n);
    }

    Argument& operator[](std::string const& name);
    Argument const& operator[](std::string const& name) const;

    ArgumentParser& name(std::string const& name);
    ArgumentParser& help(std::string const& help);

    size_t numParsedArguments() const {
        return std::count_if(
            _pimpl->arguments.begin(), _pimpl->arguments.end(),
            [](auto& pr) {
                return pr.second.isParsed();
            });
    }

    // print functions

    void printTokens() const;
    void printArguments() const;
    void printUsage() const { _pimpl->formatter.printUsage(*this); }
    void printSummary() const { _pimpl->formatter.printSummary(*this); }
    void printHelp() const { _pimpl->formatter.printHelp(*this); }

    // setters

    void setOptionPrefix(std::string const& prefix) { _pimpl->optionPrefix = prefix; }

    // getters and attributes

    std::string const& getName() const { return _pimpl->name; }
    std::string const& getHelp() const { return _pimpl->help; }
    size_t getNumRequiredChars() const { return _pimpl->numRequiredChars; }
    bool isOption(std::string const& str) const { return str.find_first_of(_pimpl->optionPrefix) == 0UL; }
    bool isOption(Argument const& arg) const { return isOption(arg.getName()); }
    bool hasSubParsers() const { return _pimpl->subparsers.has_value(); }
    bool usedSubParser(std::string const& name) const { return _pimpl->subparsers.has_value() && _pimpl->activatedSubParser == name; }

    // action

    template <typename T>
    requires ValidArgumentType<T>
    ArgType<T>& addArgument(std::string const& name);

    ArgumentGroup addMutuallyExclusiveGroup();
    SubParsers addSubParsers();

    bool parse(std::string const& line);
    bool analyzeOptions() const;

private:
    struct Token {
        Token(std::string const& tok)
            : token{tok}, parsed{false} {}
        std::string token;
        bool parsed;
    };

    struct ArgumentParserImpl {
        ArgumentParserImpl() : optionPrefix("-"), optionsAnalyzed(false) {}
        ordered_hashmap<std::string, Argument> arguments;
        std::string optionPrefix;
        std::vector<Token> tokens;

        std::vector<ArgumentGroup> mutuallyExclusiveGroups;
        std::optional<SubParsers> subparsers;
        std::string activatedSubParser;
        std::unordered_map<std::string, ArgumentGroup> mutable conflictGroups;  // map an argument name to a mutually-exclusive group if it belongs to one.

        std::string name;
        std::string help;
        size_t numRequiredChars;

        dvlab_utils::Tabler tabl;

        // members for analyzing parser options
        MyTrie mutable trie;
        bool mutable optionsAnalyzed;
        Formatter formatter;
    };

    std::shared_ptr<ArgumentParserImpl> _pimpl;

    template <typename T>
    static decltype(auto)
    operatorBracketImpl(T&& t, std::string const& name);

    // addArgument error printing

    void printDuplicateArgNameErrorMsg(std::string const& name) const;

    // pretty printing helpers

    void setNumRequiredChars(size_t num) { _pimpl->numRequiredChars = num; }
    void setSubParser(std::string const& name) {
        _pimpl->activatedSubParser = name;
        _pimpl->subparsers->setParsed(true);
    }
    ArgumentParser getActivatedSubParser() const { return _pimpl->subparsers->getSubParsers().at(_pimpl->activatedSubParser); }

    // parse subroutine
    bool tokenize(std::string const& line);
    bool parseTokens(std::span<Token>);
    bool parseOptions(std::span<Token>);
    bool parsePositionalArguments(std::span<Token>);

    static bool constraintsSatisfied(Argument const& arg);

    // parseOptions subroutine

    std::variant<std::string, size_t> matchOption(std::string const& token) const;
    void printAmbiguousOptionErrorMsg(std::string const& token) const;
    bool allRequiredOptionsAreParsed() const;
    bool allRequiredMutexGroupsAreParsed() const;

    // parsePositionalArguments subroutine

    bool allTokensAreParsed(std::span<Token>) const;
    bool allRequiredArgumentsAreParsed() const;
    void printRequiredArgumentsMissingErrorMsg() const;
};

// SECTION - On-parse actions for ArgType<T>

/**
 * @brief generate a callback that sets the argument to const value. This function
 *        should be used along with the const decorator of an argument.
 *
 * @tparam T
 * @param arg
 * @return ArgParse::ActionCallbackType
 */
template <typename T>
ActionCallbackType storeConst(ArgType<T>& arg) {
    return [&arg]() -> bool {
        arg.setValueToConst();
        return true;
    };
}

ActionCallbackType storeTrue(ArgType<bool>& arg);
ActionCallbackType storeFalse(ArgType<bool>& arg);

template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentGroup::addArgument(std::string const& name) {
    ArgType<T>& returnRef = _pimpl->_parser.addArgument<T>(name);
    _pimpl->_arguments.insert(returnRef._name);
    return returnRef;
}

}  // namespace ArgParse

#include "apArgParser.tpp"
#include "apArgument.tpp"
#include "apType.tpp"

#endif  // AP_ARGPARSE_ARGPARSER_H