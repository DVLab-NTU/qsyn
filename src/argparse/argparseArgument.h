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
    template <typename T>
    bool isOfType() const { return dynamic_cast<ArgumentModel<T>*>(_pimpl.get()) != nullptr; }

    // argument decorators
    Argument& name(std::string const& name) { _name = name; return *this; }
    template <typename T>
    Argument& defaultValue(T const& arg);
    Argument& help(std::string const& help) { _helpMessage = help; return *this; }

    // getters
    std::string const& getName() const { return _name; }
    std::string getTypeString() const { return _pimpl->doTypeString(); }
    std::optional<Argument> getDefaultValue() const { return _pimpl->doGetDefaultValue(); }
    std::string getSyntaxString() const;
    size_t getNumMandatoryChars() const { return _numMandatoryChars; }

    void printInfoString() const;

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

    void setNumMandatoryChars(size_t n) { _numMandatoryChars = n; }

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

        // getters
        virtual std::optional<Argument> doGetDefaultValue() const = 0;
    };

    /**
     * @brief type specialization of for each type of Argument.
     *
     */
    template <typename ArgType>
    struct ArgumentModel : public ArgumentConcept {
        ArgumentModel(ArgType arg)
            : _arg(std::move(arg)), _defaultValue(std::nullopt) {}

        ArgType _arg;
        std::optional<ArgType> _defaultValue;

        // type erasure indirection layer
        std::unique_ptr<ArgumentConcept> clone() const override;
        std::string doTypeString() const override;
        std::ostream& doPrint(std::ostream& os) const override;
        bool doDefaultValue(Argument const& defaultVal) override;
        std::optional<Argument> doGetDefaultValue() const override;
    };
};


//----------------------------------------
// Argument functions
//----------------------------------------

/**
 * @brief cast the `Argument` to specific type. The target type has to be of the same type as
 *        the type stored under `Argument::ArgumentModel`. The user is expected to know the 
 *        underlying type because this information is necessary for the constructor of this
 *        class. If the casting fails, throw `ArgParse::bad_arg_cast` errors. 
 * 
 * @tparam T 
 * @return T 
 */
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
 * @brief if the default value has not been set yet, set the default argument of an 
 *        parameter, and mark it as an optional argument; If there is already a default 
 *        value to the argument, this function will do nothing and warn the user. 
 *
 * @tparam T
 * @param arg the default argument to set to
 * @return Argument&
 */
template <typename T>
Argument& Argument::defaultValue(T const& defaultVal) {
    try {
        if (!_pimpl->doDefaultValue(defaultVal)) {
            std::string argStr;
            if (std::is_same_v<T, bool>) {
                argStr = (defaultVal ? "true" : "false");
            } else {
                argStr = std::to_string(defaultVal);
            }
            detail::printDuplicatedAttrErrorMsg(*this, "default value = " + argStr);
        };
    } catch (bad_arg_cast& e) {
        detail::printDefaultValueErrorMsg(*this);
    }
    return *this;
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

/**
 * @brief set the default value of an ArgumentModel. This function enables the
 *        type-erased `Argument::defaultValue()`.
 * 
 * @tparam ArgType 
 * @param defaultVal the default value of the argument
 * @return true if successfully set
 * @return false if not
 */
template <typename ArgType>
bool Argument::ArgumentModel<ArgType>::doDefaultValue(Argument const& defaultVal) {
    if (_defaultValue.has_value()) {
        return false;
    }
    _defaultValue.emplace(defaultVal);
    return true;
}

/**
 * @brief get the default value of an ArgumentModel. This function enables the
 *        type-erased `Argument::getDefaultValue()`.
 * 
 * @tparam ArgType 
 * @param defaultVal the default value of the argument
 * @return true if successfully set
 * @return false if not
 */
template <typename ArgType>
std::optional<Argument> Argument::ArgumentModel<ArgType>::doGetDefaultValue() const {
    if (_defaultValue.has_value())
        return _defaultValue.value();
    else
        return std::nullopt;
};

}  // namespace ArgParse

#endif  // QSYN_ARG_PARSE_ARGUMENT_H