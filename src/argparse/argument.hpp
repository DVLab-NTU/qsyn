/****************************************************************************
  FileName     [ argument.hpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::Argument template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <memory>

#include "argType.hpp"

namespace ArgParse {

class Argument {
public:
    Argument()
        : _pimpl{std::make_unique<Model<ArgType<DummyArgType>>>(ArgType<DummyArgType>{"dummy", DummyArgType{}})} {}

    template <typename T>
    Argument(std::string name, T val)
        : _pimpl{std::make_unique<Model<ArgType<T>>>(ArgType<T>{std::move(name), std::move(val)})} {}

    ~Argument() = default;

    Argument(Argument const& other) : _pimpl(other._pimpl->clone()) {}

    Argument& operator=(Argument copy) noexcept {
        copy.swap(*this);
        return *this;
    }
    Argument(Argument&& other) noexcept = default;

    void swap(Argument& rhs) noexcept {
        using std::swap;
        swap(_pimpl, rhs._pimpl);
    }

    friend void swap(Argument& lhs, Argument& rhs) noexcept {
        lhs.swap(rhs);
    }

    friend std::ostream& operator<<(std::ostream& os, Argument const& arg) {
        return os << fmt::format("{}", arg);
    }

    template <typename T>
    operator T() const { return get<T>(); }

    template <typename T>
    T get() const;

    std::string getTypeString() const { return _pimpl->do_getTypeString(); }
    std::string const& getName() const { return _pimpl->do_getName(); }
    std::string const& getHelp() const { return _pimpl->do_getHelp(); }
    size_t getNumRequiredChars() const { return _pimpl->do_getNumRequiredChars(); }
    std::string const& getMetavar() const { return _pimpl->do_getMetavar(); }
    NArgsRange const& getNArgs() const { return _pimpl->do_getNArgsRange(); }
    std::string toString() const { return _pimpl->do_toString(); }
    // attributes

    bool hasDefaultValue() const { return _pimpl->do_hasDefaultValue(); }
    bool isRequired() const { return _pimpl->do_isRequired(); }
    bool isParsed() const { return _pimpl->do_isParsed(); }
    bool mayTakeArgument() const { return getNArgs().upper > 0; }
    bool mustTakeArgument() const { return getNArgs().lower > 0; }

    // setters

    void setNumRequiredChars(size_t n) { _pimpl->do_setNumRequiredChars(n); }
    void setValueToDefault() { _pimpl->do_setValueToDefault(); }

    // print functions

    void printStatus() const;

    // action

    void reset();
    bool takeAction(TokensView tokens);
    bool constraintsSatisfied() const { return _pimpl->do_constraintsSatisfied(); }

    void markAsParsed() { _pimpl->do_markAsParsed(); }

private:
    friend class ArgumentParser;  // shares Argument::Model<T> and _pimpl
                                  // to ArgumentParser, enabling it to access
                                  // the underlying ArgType<T>
    friend struct fmt::formatter<Argument>;

    struct Concept {
        virtual ~Concept() {}

        virtual std::unique_ptr<Concept> clone() const = 0;

        virtual std::string do_getTypeString() const = 0;
        virtual std::string const& do_getName() const = 0;
        virtual std::string const& do_getHelp() const = 0;
        virtual std::string const& do_getMetavar() const = 0;
        virtual NArgsRange const& do_getNArgsRange() const = 0;
        virtual size_t do_getNumRequiredChars() const = 0;
        virtual void do_setNumRequiredChars(size_t) = 0;
        virtual bool do_isParsed() const = 0;
        virtual void do_markAsParsed() = 0;

        virtual bool do_hasDefaultValue() const = 0;
        virtual bool do_isRequired() const = 0;
        virtual bool do_constraintsSatisfied() const = 0;

        virtual std::string do_toString() const = 0;

        virtual bool do_takeAction(TokensView) = 0;
        virtual void do_setValueToDefault() = 0;
        virtual void do_reset() = 0;
    };

    template <typename ArgT>
    struct Model final : Concept {
        ArgT inner;

        Model(ArgT val)
            : inner(std::move(val)) {}
        ~Model() {}

        inline std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        inline std::string do_getTypeString() const override { return typeString(inner._values.front()); }
        inline std::string const& do_getName() const override { return inner._name; }
        inline std::string const& do_getHelp() const override { return inner._help; }
        inline std::string const& do_getMetavar() const override { return inner._metavar; }
        inline NArgsRange const& do_getNArgsRange() const override { return inner._nargs; }
        inline size_t do_getNumRequiredChars() const override { return inner._numRequiredChars; }
        inline void do_setNumRequiredChars(size_t n) override { inner._numRequiredChars = n; }
        inline bool do_isParsed() const override { return inner._parsed; }
        inline void do_markAsParsed() override { inner._parsed = true; }

        inline bool do_hasDefaultValue() const override { return inner._defaultValue.has_value(); }
        inline bool do_isRequired() const override { return inner._required; };
        inline bool do_constraintsSatisfied() const override { return inner.constraintsSatisfied(); }

        inline std::string do_toString() const override { return fmt::format("{}", inner); }

        inline bool do_takeAction(TokensView tokens) override { return inner.takeAction(tokens); }
        inline void do_setValueToDefault() override { return inner.setValueToDefault(); }
        inline void do_reset() override { inner.reset(); }
    };

    std::unique_ptr<Concept> _pimpl;

    TokensView getParseRange(TokensView) const;
    bool tokensEnoughToParse(TokensView) const;

    template <typename T>
    ArgType<T>& toUnderlyingType() {
        return dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())->inner;
    }
};

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
T Argument::get() const {
    if constexpr (IsContainerType<T>) {
        using V = typename std::remove_cv<typename T::value_type>::type;
        if (auto ptr = dynamic_cast<Model<ArgType<V>>*>(_pimpl.get())) {
            return ptr->inner.template get<T>();
        }
    } else {
        if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
            return ptr->inner.template get<T>();
        }
    }
    fmt::println(stderr, "[ArgParse] Error: cannot cast argument \"{}\" to target type!!", getName());
    throw std::bad_cast{};
}

}  // namespace ArgParse
