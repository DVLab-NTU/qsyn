/****************************************************************************
  FileName     [ apArgParser.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument parser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef QSYN_ARGPARSE_ARGPARSER_H
#define QSYN_ARGPARSE_ARGPARSER_H

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

class ArgumentParser;

using ActionCallbackType = std::function<bool()>;                                 // perform an action and return if it succeeds
using ErrorCallbackType = std::function<void()>;                                  // function to call when some action fails
using ConstraintCallbackType = std::pair<ActionCallbackType, ErrorCallbackType>;  // constraints are defined by an ActionCallbackType that
                                                                                  // returns true if the constraint is met, and an
                                                                                  // ErrorCallbackType that prints the error message if it does not.
struct DummyArgumentType {
    friend std::ostream& operator<<(std::ostream& os, DummyArgumentType const& val) { return os << "dummy"; }
};

namespace ArgTypeDescription {

template <typename T>
requires Arithmetic<T>
std::string getTypeString(T);  // explicitly instantiated in apType.cpp
std::string getTypeString(std::string const&);
std::string getTypeString(bool);
std::string getTypeString(DummyArgumentType);

template <typename T>
std::ostream& print(std::ostream& os, T const& val) { return os << val; }

template <typename T>
requires Arithmetic<T>
bool parseFromString(T& val, std::string const& token) { return myStr2Number<T>(token, val); }
bool parseFromString(std::string& val, std::string const& token);
bool parseFromString(bool& val, std::string const& token);
bool parseFromString(DummyArgumentType& val, std::string const& token);

}  // namespace ArgTypeDescription

template <typename T>
concept ValidArgumentType = requires(T t) {
    { ArgTypeDescription::getTypeString(t) } -> std::same_as<std::string>;
    { ArgTypeDescription::print(std::cout, t) } -> std::same_as<std::ostream&>;
    { ArgTypeDescription::parseFromString(t, std::string{}) } -> std::same_as<bool>;
};

template <typename T>
requires ValidArgumentType<T>
class ArgType {
public:
    using ActionType = std::function<ActionCallbackType(ArgType<T>&)>;
    using ErrorType = std::function<ErrorCallbackType(ArgType<T> const&)>;
    using ConstraintType = std::pair<ActionType, ErrorType>;

    ArgType(T const& val)
        : _value{val}, _traits{} {}

    friend std::ostream& operator<<(std::ostream& os, ArgType<T> const& arg) {
        return ArgTypeDescription::print(os, arg._value);
    }

    operator T&() { return _value; }
    operator T const&() const { return _value; }

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

    inline bool parse(std::string const& token);
    inline void reset();

    // getters
    inline T const& getValue() const { return _value; }
    inline std::string getTypeString() const { return ArgTypeDescription::getTypeString(_value); }
    inline std::string const& getName() const { return _traits.name; }
    inline std::string const& getHelp() const { return _traits.help; }
    inline std::optional<T> getDefaultValue() const { return _traits.defaultValue; }
    inline std::optional<T> getConstValue() const { return _traits.constValue; }
    inline std::string const& getMetaVar() const { return _traits.metavar; }
    inline std::vector<ConstraintCallbackType> const& getConstraints() const { return _traits.constraintCallbacks; }

    // setters

    void setValueToConst() {
        if (!_traits.constValue.has_value()) {
            std::cerr << "Error: no const value is specified for argument \""
                      << _traits.name << "\"!! no action is taken... \n";
            return;
        }
        _value = _traits.constValue.value();
    }

    // attributes
    inline bool hasDefaultValue() const { return _traits.defaultValue.has_value(); }
    inline bool hasConstValue() const { return _traits.constValue.has_value(); }
    inline bool hasAction() const { return _traits.actionCallback != nullptr; }
    inline bool isRequired() const { return _traits.required; }

private:
    struct Traits {
        Traits()
            : name{}, help{}, required{false}, defaultValue{std::nullopt}, constValue{}, actionCallback{}, metavar{} {}
        std::string name;
        std::string help;
        bool required;
        std::optional<T> defaultValue;
        std::optional<T> constValue;
        ActionCallbackType actionCallback;
        std::string metavar;
        std::vector<ConstraintCallbackType> constraintCallbacks;
    };

    T _value;
    Traits _traits;
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
    operator T&() const { return const_cast<T&>(get<T>()); }

    template <typename T>
    requires ValidArgumentType<T>
    operator T const&() const { return get<T>(); }

    template <typename T>
    requires ValidArgumentType<T>
    T const& get() const;

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

    template <typename T>
    struct Model final : Concept {
        T inner;

        Model(T val)
            : inner(std::move(val)) {}
        ~Model() {}

        inline std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        inline std::string do_getTypeString() const override { return inner.getTypeString(); }
        inline std::string const& do_getName() const override { return inner.getName(); }
        inline std::string const& do_getHelp() const override { return inner.getHelp(); }
        inline std::string const& do_getMetaVar() const override { return inner.getMetaVar(); }
        inline std::vector<ConstraintCallbackType> const& do_getConstraints() const override { return inner.getConstraints(); }

        inline bool do_hasDefaultValue() const override { return inner.hasDefaultValue(); }
        inline bool do_hasAction() const override { return inner.hasAction(); }
        inline bool do_isRequired() const override { return inner.isRequired(); };

        inline std::ostream& do_print(std::ostream& os) const override { return os << inner; }
        inline std::ostream& do_printDefaultValue(std::ostream& os) const override { return (inner.getDefaultValue().has_value() ? os << inner.getDefaultValue().value() : os << "(none)"); }

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
public:
    ArgumentParser() : _pimpl{std::make_shared<ArgumentParserImpl>()} {}
    ArgumentParser(std::string const& n) : _pimpl{std::make_shared<ArgumentParserImpl>()} {
        this->name(n);
    }

    Argument& operator[](std::string const& name);
    Argument const& operator[](std::string const& name) const;

    ArgumentParser& name(std::string const& name);
    ArgumentParser& help(std::string const& help);

    /**
     * @brief get the size of parsed option
     *
     * @return size_t
     */
    QSYN_ALWAYS_INLINE
    size_t numParsedArguments() const {
        return std::count_if(_pimpl->arguments.begin(), _pimpl->arguments.end(), [](auto& pr) { return pr.second.isParsed(); });
    }

    // print functions

    void printTokens() const;
    void printArguments() const;
    void printUsage() const;
    void printSummary() const;
    void printHelp() const;

    std::string getSyntaxString(Argument const& arg) const;
    std::string getSyntaxString(SubParsers parsers) const;

    // setters

    void setOptionPrefix(std::string const& prefix) { _pimpl->optionPrefix = prefix; }

    // getters and attributes

    std::string const& getName() const { return _pimpl->name; }
    std::string const& getHelp() const { return _pimpl->help; }
    size_t getNumRequiredChars() const { return _pimpl->numRequiredChars; }
    bool hasOptionPrefix(std::string const& str) const { return str.find_first_of(_pimpl->optionPrefix) == 0UL; }
    bool hasOptionPrefix(Argument const& arg) const { return hasOptionPrefix(arg.getName()); }
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

        qsutil::Tabler mutable tabl;

        // members for analyzing parser options
        MyTrie mutable trie;
        bool mutable optionsAnalyzed;
    };
    std::shared_ptr<ArgumentParserImpl> _pimpl;

    template <typename T>
    static decltype(auto)
    operatorBracketImpl(T&& t, std::string const& name);

    // addArgument error printing

    void printDuplicateArgNameErrorMsg(std::string const& name) const;

    // pretty printing helpers

    std::string requiredArgBracket(std::string const& str) const;
    std::string optionalArgBracket(std::string const& str) const;
    void printHelpString(Argument const& arg) const;
    void printHelpString(SubParsers parser) const;
    std::string styledArgName(Argument const& arg) const;
    std::string styledCmdName(std::string const& name, size_t numRequired) const;

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

// SECTION - ArgType<T> template functions

/**
 * @brief set the name of the argument
 *
 * @tparam T
 * @param name
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::name(std::string const& name) {
    _traits.name = name;
    return *this;
}

/**
 * @brief set the help message of the argument
 *
 * @tparam T
 * @param help
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::help(std::string const& help) {
    _traits.help = help;
    return *this;
}

/**
 * @brief set if the argument is required
 *
 * @tparam T
 * @param isReq
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::required(bool isReq) {
    _traits.required = isReq;
    return *this;
}

/**
 * @brief set the default value of the argument
 *
 * @tparam T
 * @param val
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::defaultValue(T const& val) {
    _traits.defaultValue = val;
    return *this;
}

/**
 * @brief set the action of the argument when parsed. An action can be
 *        any callable type that takes an `ArgType<T>&` and returns a
 *        `ArgType<T>::ActionType` (aka `std::function<bool()>`).
 *
 * @tparam T
 * @param action
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::action(ArgType<T>::ActionType const& action) {
    _traits.actionCallback = action(*this);
    return *this;
}

/**
 * @brief set the const value to store when the argument is parsed. This setting is
 *        only effective when the action is set to `ArgParse::storeConst<T>`
 *
 * @tparam T
 * @param val
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::constValue(T const& val) {
    _traits.constValue = val;
    return *this;
}

/**
 * @brief set the meta-variable, i.e., the displayed name of the argument as
 *        seen in the help message.
 *
 * @tparam T
 * @param metavar
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::metavar(std::string const& metavar) {
    _traits.metavar = metavar;
    return *this;
}

/**
 * @brief Add constraint to the argument.
 *
 * @tparam T
 * @param constraint_error a pair of constraint generator and on-error callback generator.
 *        The constraint generator takes a `ArgType<T> const&` and returns a ActionType,
 *        while takes a `ArgType<T> const&` and returns a ErrorCallbackType.
 * @return ArgType<T>&
 */
template <typename T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ConstraintType const& constraint_error) {
    return this->constraint(constraint_error.first, constraint_error.second);
}

/**
 * @brief Add constraint to the argument.
 *
 * @tparam T
 * @param constraintGen takes a `ArgType<T> const&` and returns a ActionCallbackType
 * @param onerrorGen takes a `ArgType<T> const&` and returns a ErrorCallbackType
 */
template <typename T>
ArgType<T>& ArgType<T>::constraint(ArgType<T>::ActionType const& constraint, ArgType<T>::ErrorType const& onerror) {
    if (constraint == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << getName()
                  << "\": constraint generator cannot be nullptr!!" << std::endl;
        return *this;
    }
    ActionCallbackType constraintCallback = constraint(*this);
    if (constraintCallback == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << getName()
                  << "\": constraint generator does not produce valid callback!!" << std::endl;
        return *this;
    }
    ErrorCallbackType onerrorCallback;
    if (onerror == nullptr) {
        onerrorCallback = [this]() {
            std::cerr << "Error: invalid value \"" << getValue() << "\" for argument \""
                      << getName() << "\": fail to satisfy constraint(s)!!" << std::endl;
        };
    } else {
        onerrorCallback = onerror(*this);
    }

    if (onerrorCallback == nullptr) {
        std::cerr << "[ArgParse] Failed to add constraint to argument \"" << getName()
                  << "\": error callback generator does not produce valid callback!!" << std::endl;
        return *this;
    }

    _traits.constraintCallbacks.emplace_back(constraintCallback, onerrorCallback);
    return *this;
}

template <typename T>
ArgType<T>& ArgType<T>::choices(std::initializer_list<T> const& choices) {
    std::vector<T> vec{choices};
    auto constraint = [&vec](ArgType<T> const& arg) -> ActionCallbackType {
        return [&arg, vec]() {
            return any_of(vec.begin(), vec.end(), [&arg](T const& choice) -> bool {
                return arg.getValue() == choice;
            });
        };
    };
    auto error = [&vec](ArgType<T> const& arg) -> ErrorCallbackType {  // REVIEW - iostream in header... is there any way to resolve this?
        return [&arg, vec]() {
            std::cerr << "Error: invalid choice for argument \"" << arg.getName() << "\": "
                      << "please choose from {";
            size_t ctr = 0;
            for (auto& choice : vec) {
                if (ctr > 0) std::cerr << ", ";
                std::cerr << choice;
                ctr++;
            }
            std::cerr << "}!!\n";
        };
    };

    return this->constraint(constraint, error);
}

/**
 * @brief If the argument has a default value, reset to it.
 *
 */
template <typename T>
void ArgType<T>::reset() {
    if (hasDefaultValue()) _value = _traits.defaultValue.value();
}

/**
 * @brief parse the argument. If the argument has an action, perform it; otherwise,
 *        try to parse the value from token.
 *
 * @param token
 * @return true if succeeded
 * @return false if failed
 */
template <typename T>
bool ArgType<T>::parse(std::string const& token) {
    if (hasAction()) return _traits.actionCallback();
    return ArgTypeDescription::parseFromString(_value, token);
}

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

// SECTION - Argument Template Functions

/**
 * @brief Access the data stored in the argument.
 *        This function only works when the target type T is
 *        the same as the stored type; otherwise, this function
 *        throws an error.
 *
 * @tparam T the stored data type
 * @return T const&
 */
template <typename T>
requires ValidArgumentType<T>
T const& Argument::get() const {
    if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
        return ptr->inner;
    }

    std::cerr << "[ArgParse] Error: cannot cast argument \""
              << getName() << "\" to target type!!\n";
    throw std::bad_any_cast{};
}

// SECTION - ArgumentParser template functions

/**
 * @brief add an argument with the name.
 *
 * @tparam T
 * @param name
 * @return ArgType<T>&
 */
template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentParser::addArgument(std::string const& name) {
    auto realname = toLowerString(name);
    if (_pimpl->arguments.contains(realname)) {
        printDuplicateArgNameErrorMsg(name);
    } else {
        _pimpl->arguments.emplace(realname, Argument(T{}));
    }

    ArgType<T>& returnRef = dynamic_cast<Argument::Model<ArgType<T>>*>(_pimpl->arguments.at(realname)._pimpl.get())->inner;

    if (!hasOptionPrefix(realname)) {
        returnRef.required(true).metavar(realname);
    } else {
        returnRef.metavar(toUpperString(realname.substr(realname.find_first_not_of(_pimpl->optionPrefix))));
    }

    _pimpl->optionsAnalyzed = false;

    return returnRef.name(name);
}

template <typename T>
decltype(auto)
ArgumentParser::operatorBracketImpl(T&& t, std::string const& name) {
    if (t._pimpl->subparsers.has_value() && t._pimpl->subparsers->isParsed()) {
        auto subparser = t.getActivatedSubParser();
        subparser.printArguments();
        if (subparser._pimpl->arguments.contains(toLowerString(name))) {
            return subparser._pimpl->arguments.at(toLowerString(name));
        }
    }
    try {
        return std::forward<T>(t)._pimpl->arguments.at(toLowerString(name));
    } catch (std::out_of_range& e) {
        std::cerr << "Argument name \"" << name
                  << "\" does not exist for command \""
                  << std::forward<T>(t).styledCmdName(std::forward<T>(t).getName(), std::forward<T>(t).getNumRequiredChars()) << "\"\n";
        throw e;
    }
}

template <typename T>
requires ValidArgumentType<T>
ArgType<T>& ArgumentGroup::addArgument(std::string const& name) {
    ArgType<T>& returnRef = _pimpl->_parser.addArgument<T>(name);
    _pimpl->_arguments.insert(returnRef.getName());
    return returnRef;
}

}  // namespace ArgParse

#endif  // QSYN_ARGPARSE_ARGPARSER_H