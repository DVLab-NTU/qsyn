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
#include <iosfwd>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "argparseDef.h"
#include "argparseErrorMsg.h"

namespace ArgParse {

class Argument;

/**
 * @brief per-type implementation to enable type-erasure
 *
 */
namespace detail {

// argument type: bool

std::string getTypeString(bool const& arg);

// argument type: int

std::string getTypeString(int const& arg);

// argument type: std::string

std::string getTypeString(std::string const& arg);

}  // namespace detail

/**
 * @brief A type-erased interface to all argument types that
 *        `ArgParse::ArgParser` admits.
 *
 */
class Argument {
public:
    template <typename ArgType>
    Argument(ArgType arg)
        : _pimpl{std::make_unique<ArgumentModel<ArgType>>(std::move(arg))},
          _name(std::string{}) {}

    template <size_t N>
    Argument(char const (&arg)[N])
        : _pimpl{std::make_unique<ArgumentModel<std::string>>(std::move(std::string(arg)))},
          _name(std::string{}) {}

    Argument(Argument const& other)
        : _pimpl(other._pimpl->clone()), _name(other._name) {}

    Argument& operator=(Argument const& other);
    friend std::ostream& operator<<(std::ostream& os, Argument const& arg);

    template <typename T>
    operator T() const;

    // attribute
    bool isMandatory() const { return !getDefaultValue().has_value(); }
    bool isOptional() const { return getDefaultValue().has_value(); }

    bool isFlag() const { return getFlagValue().has_value(); }

    // argument decorators
    template <typename T>
    Argument& defaultValue(T const& arg);

    template <typename T>
    Argument& flag(T const& arg);
    Argument& help(std::string const& help) {
        _helpMessage = help;
        return *this;
    }

    // setters / getters
    void setName(std::string const& name) { _name = name; }
    void setNumMandatoryChars(size_t n) { _numMandatoryChars = n; }

    std::string const& getName() const { return _name; }
    std::string getTypeString() const { return _pimpl->doTypeString(); }
    std::optional<Argument> getDefaultValue() const { return _pimpl->doGetDefaultValue(); }
    std::optional<Argument> getFlagValue() const { return _pimpl->doGetFlagValue(); }
    std::string getSyntaxString() const;
    size_t getNumMandatoryChars() const { return _numMandatoryChars; }

    void printInfoString() const;

    Argument() {}

private:
    // only use for dummy return

    friend class ArgumentParser;
    struct ArgumentConcept;

    std::unique_ptr<ArgumentConcept> _pimpl;
    std::string _name;
    size_t _numMandatoryChars;
    std::string _helpMessage;

    // pretty printing helpers

    std::string typeBracket(std::string const& str) const;
    std::string optionBracket(std::string const& str) const;
    std::string formattedType() const;
    std::string formattedName() const;

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
        virtual bool doDefaultValue(Argument const& defaultVal) = 0;
        virtual void doFlag(Argument const& flagVal) = 0;

        // getters
        virtual std::optional<Argument> doGetDefaultValue() const = 0;
        virtual std::optional<Argument> doGetFlagValue() const = 0;
    };

    /**
     * @brief type specialization of for each type of Argument.
     *
     */
    template <typename ArgType>
    struct ArgumentModel : public ArgumentConcept {
        ArgumentModel(ArgType arg)
            : _arg(std::move(arg)), _defaultValue(std::nullopt), _flagValue(std::nullopt) {}

        ArgType _arg;
        std::optional<ArgType> _defaultValue;
        std::optional<ArgType> _flagValue;

        // type erasure indirection layer
        std::unique_ptr<ArgumentConcept> clone() const override;
        std::string doTypeString() const override;
        std::ostream& doPrint(std::ostream& os) const override;
        bool doDefaultValue(Argument const& defaultVal) override {
            if (_defaultValue.has_value()) {
                return false;
            }
            _defaultValue.emplace(defaultVal);
            return true;
        }

        void doFlag(Argument const& flagVal) override {
            _flagValue.emplace(flagVal);
        }

        std::optional<Argument> doGetDefaultValue() const override {
            if (_defaultValue.has_value())
                return _defaultValue.value();
            else
                return std::nullopt;
        };
        std::optional<Argument> doGetFlagValue() const override {
            if (_flagValue.has_value())
                return _flagValue.value();
            else
                return std::nullopt;
        };
    };
};

template <typename T>
Argument::operator T() const {
    if (auto ptr = dynamic_cast<ArgumentModel<T>*>(_pimpl.get())) {
        return T(ptr->_arg);
    } else {
        detail::printArgumentCastErrorMsg(*this);
        throw bad_arg_cast{};
    }
}

/**
 * @brief Set the default argument of an parameter
 *
 * @tparam T
 * @param arg the default argument to set to
 * @return Argument&
 */
template <typename T>
Argument& Argument::defaultValue(T const& arg) {
    try {
        if (!_pimpl->doDefaultValue(arg)) {
            std::string argStr;
            if (std::is_same_v<T, bool>) {
                argStr = (arg ? "true" : "false");
            } else {
                argStr = std::to_string(arg);
            }
            detail::printDuplicatedAttrErrorMsg(*this, "default value = " + argStr);
        };
    } catch (bad_arg_cast& e) {
        detail::printDefaultValueErrorMsg(*this);
    }
    return *this;
}

/**
 * @brief Set the default argument of an parameter
 *
 * @tparam T
 * @param arg the default argument to set to
 * @return Argument&
 */
template <typename T>
Argument& Argument::flag(T const& arg) {
    try {
        _pimpl->doFlag(arg);
    } catch (bad_arg_cast& e) {
        detail::printDefaultValueErrorMsg(*this);
    }
    return *this;
}

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