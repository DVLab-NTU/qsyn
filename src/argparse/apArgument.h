/****************************************************************************
  FileName     [ argument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument interface for ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARGPARSE_ARGUMENT_H
#define QSYN_ARGPARSE_ARGUMENT_H

#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "myConcepts.h"
#include "util.h"

namespace ArgParse {

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
class ArgType {
public:
    using ActionType = std::function<ActionCallbackType(ArgType<T>&)>;
    using ErrorType = std::function<ErrorCallbackType(ArgType<T> const&)>;
    using ConstraintType = std::pair<ActionType, ErrorType>;

    ArgType(T const& val) : _value{val}, _traits{} {}

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

    void reset();
    bool parse(std::string const& token);

    // getters
    T const& getValue() const { return _value; }
    std::string getTypeString() const { return ArgTypeDescription::getTypeString(_value); }
    std::string const& getName() const { return _traits.name; }
    std::string const& getHelp() const { return _traits.help; }
    std::optional<T> getDefaultValue() const { return _traits.defaultValue; }
    std::optional<T> getConstValue() const { return _traits.constValue; }
    std::string const& getMetaVar() const { return _traits.metavar; }
    std::vector<ConstraintCallbackType> const& getConstraints() const { return _traits.constraintCallbacks; }

    // setters

    void setValueToConst() {
        if (!_traits.constValue.has_value()) {
            std::cerr << "Error: no const value is specified for argument \""
                      << _traits.name << "\"!! no action is taken... \n";
        }
        _value = _traits.constValue.value();
    }

    // attributes
    bool hasDefaultValue() const { return _traits.defaultValue.has_value(); }
    bool hasConstValue() const { return _traits.constValue.has_value(); }
    bool hasAction() const { return _traits.actionCallback != nullptr; }
    bool isRequired() const { return _traits.required; }

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
    Argument() : _pimpl{std::make_unique<Model<ArgType<DummyArgumentType>>>(DummyArgumentType{})} {}

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
    operator T&() const {
        return const_cast<T&>(get<T>());
    }

    template <typename T>
    operator T const&() const {
        return get<T>();
    }

    template <typename T>
    T const& get() const;

    std::string getTypeString() const { return _pimpl->do_getTypeString(); }
    std::string const& getName() const { return _pimpl->do_getName(); }
    std::string const& getHelp() const { return _pimpl->do_getHelp(); }
    size_t getNumRequiredChars() const { return _numRequiredChars; }
    std::string const& getMetavar() const { return _pimpl->do_getMetavar(); }
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
        virtual std::string const& do_getMetavar() const = 0;
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

        Model(T val) : inner(std::move(val)) {}
        ~Model() {}

        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        std::string do_getTypeString() const override { return inner.getTypeString(); }
        std::string const& do_getName() const override { return inner.getName(); }
        std::string const& do_getHelp() const override { return inner.getHelp(); }
        std::string const& do_getMetavar() const override { return inner.getMetaVar(); }
        std::vector<ConstraintCallbackType> const& do_getConstraints() const override { return inner.getConstraints(); }

        bool do_hasDefaultValue() const override { return inner.hasDefaultValue(); }
        bool do_hasAction() const override { return inner.hasAction(); }
        bool do_isRequired() const override { return inner.isRequired(); };

        std::ostream& do_print(std::ostream& os) const override { return os << inner; }
        std::ostream& do_printDefaultValue(std::ostream& os) const override { return (inner.getDefaultValue().has_value() ? os << inner.getDefaultValue().value() : os << "(none)"); }

        bool do_parse(std::string const& token) override { return inner.parse(token); }
        void do_reset() override { inner.reset(); }
    };

    std::unique_ptr<Concept> _pimpl;

    bool _parsed;
    size_t _numRequiredChars;
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
T const& Argument::get() const {
    if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
        return ptr->inner;
    }

    std::cerr << "[ArgParse] Error: cannot cast argument \""
              << getName() << "\" to target type!!\n";
    throw std::bad_any_cast{};
}

}  // namespace ArgParse

#endif  // QSYN_ARGPARSE_ARGUMENT_H