/****************************************************************************
  FileName     [ argument.hpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::Argument template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <memory>

#include "argType.hpp"

namespace ArgParse {

class Argument {                  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : copy-swap idiom
    friend class ArgumentParser;  // shares Argument::Model<T> and _pimpl
                                  // to ArgumentParser, enabling it to access
                                  // the underlying ArgType<T>

    friend struct fmt::formatter<Argument>;
    friend class Formatter;

public:
    ~Argument() = default;
    Argument(Argument const& other) : _pimpl(other._pimpl->clone()), _isOption(other._isOption) {}

    Argument& operator=(Argument copy) noexcept {
        copy.swap(*this);
        return *this;
    }

    Argument(Argument&& other) noexcept = default;

    void swap(Argument& rhs) noexcept {
        std::swap(_pimpl, rhs._pimpl);
    }

    friend void swap(Argument& lhs, Argument& rhs) noexcept {
        lhs.swap(rhs);
    }

    // getters
    std::string getTypeString() const { return _pimpl->do_getTypeString(); }
    std::string const& getName() const { return _pimpl->do_getName(); }
    std::optional<std::string> const& getUsage() const { return _pimpl->do_getUsage(); }
    std::string const& getHelp() const { return _pimpl->do_getHelp(); }
    std::string const& getMetavar() const { return _pimpl->do_getMetavar(); }
    NArgsRange const& getNArgs() const { return _pimpl->do_getNArgsRange(); }
    std::string toString() const { return _pimpl->do_toString(); }

    // attributes
    bool hasDefaultValue() const { return _pimpl->do_hasDefaultValue(); }
    bool isRequired() const { return _pimpl->do_isRequired() && (_isOption || getNArgs().lower > 0); }
    bool isOption() const { return _isOption; }
    bool isHelpAction() const { return _pimpl->do_isHelpAction(); }
    bool isVersionAction() const { return _pimpl->do_isVersionAction(); }

private:
    Argument()
        : _pimpl{std::make_unique<Model<ArgType<DummyArgType>>>(ArgType<DummyArgType>{"dummy", DummyArgType{}})} {}

    template <typename T>
    Argument(std::string name, T val)
        : _pimpl{std::make_unique<Model<ArgType<T>>>(ArgType<T>{std::move(name), std::move(val)})} {}

    template <typename T>
    T get() const;

    // setters
    void setValueToDefault() { _pimpl->do_setValueToDefault(); }

    // print functions
    void printStatus() const;

    // action
    void reset();
    bool takeAction(TokensView tokens);
    bool constraintsSatisfied() const { return _pimpl->do_constraintsSatisfied(); }
    void markAsParsed() { _pimpl->do_markAsParsed(); }

    struct Concept {  // NOLINT(hicpp-special-member-functions, cppcoreguidelines-special-member-functions) : pure-virtual interface
        virtual ~Concept() {}

        virtual std::unique_ptr<Concept> clone() const = 0;

        virtual std::string do_getTypeString() const = 0;
        virtual std::string const& do_getName() const = 0;
        virtual std::optional<std::string> const& do_getUsage() const = 0;
        virtual std::string const& do_getHelp() const = 0;
        virtual std::string const& do_getMetavar() const = 0;
        virtual NArgsRange const& do_getNArgsRange() const = 0;
        virtual bool do_isParsed() const = 0;
        virtual void do_markAsParsed() = 0;

        virtual bool do_hasDefaultValue() const = 0;
        virtual bool do_isRequired() const = 0;
        virtual bool do_isHelpAction() const = 0;
        virtual bool do_isVersionAction() const = 0;
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

        inline std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        inline std::string do_getTypeString() const override {
            using V = typename std::remove_cv<typename decltype(inner._values)::value_type>::type;
            return typeString(V{});
        }
        inline std::string const& do_getName() const override { return inner._name; }
        inline std::optional<std::string> const& do_getUsage() const override { return inner._usage; }
        inline std::string const& do_getHelp() const override { return inner._help; }
        inline std::string const& do_getMetavar() const override { return inner._metavar; }
        inline NArgsRange const& do_getNArgsRange() const override { return inner._nargs; }
        inline bool do_isParsed() const override { return inner._parsed; }
        inline void do_markAsParsed() override { inner._parsed = true; }

        inline bool do_hasDefaultValue() const override { return inner._defaultValue.has_value(); }
        inline bool do_isRequired() const override { return inner._required; }
        inline bool do_isHelpAction() const override { return inner._isHelpAction; }
        inline bool do_isVersionAction() const override { return inner._isVersionAction; }
        inline bool do_constraintsSatisfied() const override { return inner.constraintsSatisfied(); }

        inline std::string do_toString() const override { return fmt::format("{}", inner); }

        inline bool do_takeAction(TokensView tokens) override { return inner.takeAction(tokens); }
        inline void do_setValueToDefault() override { return inner.setValueToDefault(); }
        inline void do_reset() override { inner.reset(); }
    };

    std::unique_ptr<Concept> _pimpl;
    bool _isOption = false;

    bool isParsed() const { return _pimpl->do_isParsed(); }
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
    exit(1);
}

}  // namespace ArgParse

namespace fmt {

template <>
struct formatter<ArgParse::Argument> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(ArgParse::Argument const& arg, FormatContext& ctx) -> format_context::iterator {
        return fmt::format_to(ctx.out(), "{}", arg.toString());
    }
};

}  // namespace fmt
