/****************************************************************************
  FileName     [ argparseArgument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define class ArgParse::Argument member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARG_PARSE_ARGUMENT_H
#define QSYN_ARG_PARSE_ARGUMENT_H

#include <any>
#include <concepts>
#include <functional>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "argparseArgTypes.h"
#include "argparseDef.h"
#include "argparseErrorMsg.h"

namespace ArgParse {

/**
 * @brief A type-erased interface to all argument types that
 *        `ArgParse::ArgParser` admits.
 *
 */
class Argument {
public:
    using ActionType = std::function<bool(Argument&)>;
    using OnErrorCallbackType = std::function<void(Argument const&)>;
    using ConstraintType = std::pair<ActionType, OnErrorCallbackType>;

    // construct/assign from internal types
    template <typename ArgType>
    explicit Argument(ArgType arg)
        : _pimpl{std::make_unique<ArgumentModel<ArgType>>(std::move(arg))},
          _traits() {}

    template <size_t N>
    explicit Argument(char const (&arg)[N])
        : _pimpl{std::make_unique<ArgumentModel<std::string>>(std::move(std::string(arg)))},
          _traits() {}

    template <typename ArgType>
    Argument& operator=(ArgType const& other) {
        _pimpl = std::make_unique<ArgumentModel<ArgType>>(std::move(other));
        return *this;
    }

    // construct/assign from other arguments

    Argument(Argument const& other)
        : _pimpl(other._pimpl->clone()), _traits(other._traits) {}

    Argument& operator=(Argument const& other) {
        other._pimpl->clone().swap(_pimpl);
        _traits = other._traits;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, Argument const& arg);

    template <typename T>
    operator T&() const;

    template <typename T>
    operator T const&() const;

    // argument decorators

    Argument& name(std::string const& name);
    Argument& metavar(std::string const& mvar);
    Argument& required();
    Argument& optional();
    Argument& help(std::string const& help);
    Argument& action(ActionType const& action);
    Argument& constraint(ActionType const& constraint, OnErrorCallbackType const& onerror);

    template <typename T>
    Argument& defaultValue(T const& val);
    template <typename T>
    Argument& choices(std::initializer_list<T> const& choices);

    // parse

    bool parse(std::span<TokenPair> tokens);

    // getters

    std::string const& getName() const { return _traits.name; }
    std::string const& getMetaVar() const { return _traits.metavar; }
    std::string const& getHelp() const { return _traits.help; }
    std::string getTypeString() const { return hasAction() ? "flag" : _pimpl->doTypeString(); }
    size_t getNumMandatoryChars() const { return _traits.numMandatoryChars; }
    ActionType const& getAction() const { return _traits.action; }
    ActionType const& getResetCallback() const { return _traits.resetCallback; }
    std::vector<ConstraintType> const& getConstraintCallbacks() const { return _traits.constraintCallbacks; }

    // attributes

    template <typename T>
    bool isOfType() const { return dynamic_cast<ArgumentModel<T>*>(_pimpl.get()) != nullptr; }

    bool isRequired() const { return _traits.required; }
    bool isOptional() const { return !isRequired(); }
    bool isPositional() const { return !isNonPositional(); }
    bool isNonPositional() const { return _traits.name.starts_with("-"); }
    bool isParsed() const { return _traits.parsed; }

    bool hasDefaultValue() const { return _traits.hasDefaultVal; }
    bool hasAction() const { return _traits.action != nullptr; }
    bool hasResetCallback() const { return _traits.resetCallback != nullptr; }

    // print functions

    void printHelpString() const;
    void printStatus() const;

    // common actions

    template <typename T>
    static ActionType storeConst(T const& constant);
    static ActionType storeTrue() { return storeConst(true); }
    static ActionType storeFalse() { return storeConst(false); }

private:
    // only use for dummy return

    friend class ArgumentParser;
    struct ArgumentConcept;

    std::unique_ptr<ArgumentConcept> _pimpl;

    struct ArgumentTraits {
        ArgumentTraits()
            : name{}, metavar{}, help{}, numMandatoryChars{0}, parsed{false}, required{true}, hasDefaultVal{false}, action{}, resetCallback{}, constraintCallbacks{} {}
        std::string name;
        std::string metavar;
        std::string help;
        size_t numMandatoryChars;

        bool parsed;
        bool required;
        bool hasDefaultVal;
        ActionType action;
        ActionType resetCallback;
        std::vector<ConstraintType> constraintCallbacks;
    };

    ArgumentTraits _traits;

    // pretty printing helpers
    std::string getSyntaxString() const;
    std::string typeBracket(std::string const& str) const;
    std::string optionBracket(std::string const& str) const;
    std::string formattedType() const;
    std::string formattedName() const;
    std::string formattedMetaVar() const;

    // setters
    void reset() {
        setParsed(false);
        if (hasResetCallback()) getResetCallback()(*this);
    }

    template <typename T>
    void setDefaultValue(T const& val) {
        // convert to tokens first to circumvent type issues
        auto vec = detail::toTokens(val);
        _pimpl->doParse(vec);
        _traits.required = false;
        _traits.hasDefaultVal = true;
        _traits.resetCallback = resetToDefault(val);
    }
    void setName(std::string const& name) { _traits.name = name; }
    void setMetavar(std::string const& mvar) {_traits.metavar = mvar; }
    void setHelp(std::string const& help) { _traits.help = help; }
    void setAction(ActionType const& action) { _traits.action = action; }
    void setNumMandatoryChars(size_t n) { _traits.numMandatoryChars = n; }
    void setRequired(bool isReq) { _traits.required = isReq; }
    void setParsed(bool parsed) { _traits.parsed = parsed; }

    void addConstraint(ActionType const& constraint, OnErrorCallbackType const& onerror = nullptr) {
        _traits.constraintCallbacks.emplace_back(constraint, onerror);
    }

    template <typename T>
    static ActionType resetToDefault(T const& val);

    template <typename T>
    ActionType makeChoicesConstraint(std::vector<T> vecChoices) {
        return [vecChoices](Argument const& arg) -> bool {
            return any_of(vecChoices.begin(), vecChoices.end(), [&arg](T const& val) {
                return (val == (T&)arg);
            });
        };
    }

    /**
     * @brief Interface representing the concept of an Argument.
     *
     */
    struct ArgumentConcept {
        virtual ~ArgumentConcept() {}

        // type erasure indirection layer
        virtual std::unique_ptr<ArgumentConcept> clone() const = 0;
        virtual std::string doTypeString() const = 0;
        virtual std::ostream& doPrint(std::ostream& os) const = 0;
        virtual bool doParse(std::span<TokenPair> tokens) = 0;
    };

    /**
     * @brief type specialization of for each type of Argument.
     *
     */
    template <typename ArgType>
    struct ArgumentModel : public ArgumentConcept {
        ArgumentModel(ArgType arg)
            : _arg(std::move(arg)) {}

        ArgType _arg;

        // type erasure indirection layer
        std::unique_ptr<ArgumentConcept> clone() const override;
        std::string doTypeString() const override;
        std::ostream& doPrint(std::ostream& os) const override;
        bool doParse(std::span<TokenPair> tokens) override {
            return detail::parse(_arg, tokens);
        }
    };
};

//----------------------------------------
// Argument functions
//----------------------------------------

/**
 * @brief cast the `Argument` to specific type. The target type has to be of the same type as
 *        the type stored under `Argument::ArgumentModel`. The user is expected to know the
 *        underlying type. If the casting fails, throw `ArgParse::bad_arg_cast` errors.
 *
 * @tparam T
 * @return T
 */
template <typename T>
Argument::operator T&() const {
    if (auto ptr = dynamic_cast<ArgumentModel<T>*>(_pimpl.get())) {
        if (!isParsed() && !hasDefaultValue())
            detail::printArgumentUnparsedErrorMsg(*this);
        return ptr->_arg;
    } else {
        detail::printArgumentCastErrorMsg(*this);
        throw bad_arg_cast{};
    }
}

/**
 * @brief cast the `Argument` to specific type. The target type has to be of the same type as
 *        the type stored under `Argument::ArgumentModel`. The user is expected to know the
 *        underlying type. If the casting fails, throw `ArgParse::bad_arg_cast` errors.
 *
 * @tparam T
 * @return T
 */
template <typename T>
Argument::operator T const&() const {
    if (auto ptr = dynamic_cast<ArgumentModel<T>*>(_pimpl.get())) {
        if (!isParsed() && !hasDefaultValue())
            detail::printArgumentUnparsedErrorMsg(*this);
        return ptr->_arg;
    } else {
        detail::printArgumentCastErrorMsg(*this);
        throw bad_arg_cast{};
    }
}

/**
 * @brief set default value to an argument. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
template <typename T>
Argument& Argument::defaultValue(T const& val) {
    setDefaultValue(val);
    return *this;
}

/**
 * @brief set choices to an argument. 
 *        This function returns the reference to the argument to ease decorator chaining
 * 
 * @tparam T 
 * @param val 
 * @return Argument& 
 */
template <typename T>
Argument& Argument::choices(std::initializer_list<T> const& choices) {
    if constexpr (std::is_same_v<T, char const*>) {
        std::vector<std::string> vecChoices(choices.begin(), choices.end());
        addConstraint(makeChoicesConstraint<std::string>(vecChoices),
                        detail::printParseResultIsNotAChoiceErrorMsg);
    } else {
        std::vector<T> vecChoices{choices};
        addConstraint(makeChoicesConstraint<T>(vecChoices),
                        detail::printParseResultIsNotAChoiceErrorMsg);
    }
    return *this;
}

template <typename T>
Argument::ActionType Argument::storeConst(T const& constant) {
    // must pass `constant` by copy so the callback remenber its state!
    return [constant](Argument& arg) -> bool {
        try {
            arg = constant;
        } catch (bad_arg_cast& e) {
            detail::printArgumentCastErrorMsg(arg);
            return false;
        }
        return true;
    };
}

template <typename T>
Argument::ActionType Argument::resetToDefault(T const& val) {
    // must pass `val` by copy so the callback remenber its state!
    return [val](Argument& arg) -> bool {
        try {
            // convert to token first to circumvent type issues
            auto vec = detail::toTokens(val);
            arg._pimpl->doParse(vec);
        } catch (bad_arg_cast& e) {
            detail::printArgumentCastErrorMsg(arg);
            return false;
        }
        return true;
    };
}

//----------------------------------------
// Argument::ArgumentModel functions
//----------------------------------------

/**
 * @brief clone an ArgumentModel. This function enables the
 *        type-erased copying of class `Argument`.
 *
 * @tparam ArgType
 * @return std::unique_ptr<Argument::ArgumentConcept >
 */
template <typename ArgType>
std::unique_ptr<Argument::ArgumentConcept> Argument::ArgumentModel<ArgType>::clone() const {
    return std::make_unique<ArgumentModel>(*this);
}

/**
 * @brief get the type string of an ArgumentModel. This function enables the
 *        type-erased `Argument::getTypeString()`.
 *
 * @tparam ArgType
 * @return std::unique_ptr<Argument::ArgumentConcept >
 */
template <typename ArgType>
std::string Argument::ArgumentModel<ArgType>::doTypeString() const {
    return detail::getTypeString(_arg);
}

/**
 * @brief print an ArgumentModel. This function enables the
 *        type-erased `std::ostream& operator<< (std::ostream&, Argument const&)`.
 *
 * @tparam ArgType
 * @return std::unique_ptr<Argument::ArgumentConcept >
 */
template <typename ArgType>
std::ostream& Argument::ArgumentModel<ArgType>::doPrint(std::ostream& os) const {
    return os << _arg;
}

}  // namespace ArgParse

#endif  // QSYN_ARG_PARSE_ARGUMENT_H