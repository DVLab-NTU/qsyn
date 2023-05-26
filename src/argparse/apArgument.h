/****************************************************************************
  FileName     [ argument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument interface for ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef QSYN_ARGPARSE_ARGUMENT_H
#define QSYN_ARGPARSE_ARGUMENT_H

#include <functional>
#include <memory>
#include <span>

#include "apArgType.h"

namespace ArgParse {

class ArgumentParser;

struct DummyArgumentType {
    DummyArgumentType() : name{"dummy"} {}
    std::string const& getTypeString() const { return name; }
    std::string const& getName() const { return name; }
    std::string const& getHelp() const { return name; }
    std::string const& getMetaVar() const { return name; }
    std::vector<ConstraintCallbackType> const& getConstraints() const { return constraints; }

    bool hasDefaultValue() const { return false; }
    bool hasAction() const { return false; }
    bool isRequired() const { return false; }

    friend std::ostream& operator<<(std::ostream& os, DummyArgumentType const& val) {
        return os;
    }

    std::optional<DummyArgumentType> getDefaultValue() const { return std::nullopt; }
    bool parse(std::string const&) { return true; }
    void reset() { return; }

    std::string name;
    std::vector<ConstraintCallbackType> constraints;
};

class Argument {
public:
    Argument() : _pimpl{std::make_unique<Model<DummyArgumentType>>(DummyArgumentType{})} {}
    template <typename T>
    Argument(T const& val)
        : _pimpl{std::make_unique<Model<ArgType<T>>>(std::move(val))}, _parsed{false}, _numRequiredChars{1} {}

    ~Argument() = default;

    Argument(Argument const& other)
        : _pimpl(other._pimpl->clone()), _parsed{other._parsed}, _numRequiredChars{other._numRequiredChars} {}

    Argument& operator=(Argument const& other) {
        other._pimpl->clone().swap(_pimpl);
        _parsed = other._parsed;
        _numRequiredChars = other._numRequiredChars;
        return *this;
    }

    Argument(Argument&& other) = default;
    Argument& operator=(Argument&& other) = default;

    // deliberately left out move ctors and assignments. The copy counterparts serves as a fallback

    friend std::ostream& operator<<(std::ostream& os, Argument const& arg) {
        return arg._pimpl->doPrint(os);
    }

    // enable casting back to original type

    template <typename T>
    operator T&() const {
        if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
            return ptr->inner;
        }

        printArgCastErrorMsg();
        exit(-1);
    }

    template <typename T>
    operator T const&() const {
        if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
            return ptr->inner;
        }

        printArgCastErrorMsg();
        exit(-1);
    }

    // getters

    std::string getTypeString() const { return _pimpl->doGetTypeString(); }
    std::string const& getName() const { return _pimpl->doGetName(); }
    std::string const& getHelp() const { return _pimpl->doGetHelp(); }
    size_t getNumRequiredChars() const { return _numRequiredChars; }
    std::string const& getMetavar() const { return _pimpl->doGetMetaVar(); }
    std::vector<ConstraintCallbackType> const& getConstraints() const { return _pimpl->doGetConstraints(); }

    // attributes

    bool hasDefaultValue() const { return _pimpl->doHasDefaultValue(); }
    bool hasAction() const { return _pimpl->doHasAction(); }
    bool isRequired() const { return _pimpl->doIsRequired(); }
    bool isParsed() const { return _parsed; }

    // setters

    void setNumRequiredChars(size_t n) { _numRequiredChars = n; }

    // print functions

    void printStatus() const;
    void printDefaultValue(std::ostream& os) const { _pimpl->doPrintDefaultValue(os); }

    // action

    void reset();
    bool parse(std::string const& token);

private:
    friend class ArgumentParser;

    struct Concept;
    std::unique_ptr<Concept> _pimpl;

    bool _parsed;
    size_t _numRequiredChars;

    void printArgCastErrorMsg() const;

    struct Concept {
        virtual ~Concept() {}

        virtual std::unique_ptr<Concept> clone() const = 0;

        virtual std::string doGetTypeString() const = 0;
        virtual std::string const& doGetName() const = 0;
        virtual std::string const& doGetHelp() const = 0;
        virtual std::string const& doGetMetaVar() const = 0;
        virtual std::vector<ConstraintCallbackType> const& doGetConstraints() const = 0;

        virtual bool doHasDefaultValue() const = 0;
        virtual bool doHasAction() const = 0;
        virtual bool doIsRequired() const = 0;

        virtual std::ostream& doPrint(std::ostream& os) const = 0;
        virtual std::ostream& doPrintDefaultValue(std::ostream& os) const = 0;

        virtual bool doParse(std::string const& token) = 0;
        virtual void doReset() = 0;
    };

    template <typename T>
    struct Model final : Concept {
        T inner;

        Model(T val) : inner(std::move(val)) {}
        ~Model() {}

        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        std::string doGetTypeString() const override { return inner.getTypeString(); }
        std::string const& doGetName() const override { return inner.getName(); }
        std::string const& doGetHelp() const override { return inner.getHelp(); }
        std::string const& doGetMetaVar() const override { return inner.getMetaVar(); }
        std::vector<ConstraintCallbackType> const& doGetConstraints() const override { return inner.getConstraints(); }

        bool doHasDefaultValue() const override { return inner.hasDefaultValue(); }
        bool doHasAction() const override { return inner.hasAction(); }
        bool doIsRequired() const override { return inner.isRequired(); };

        std::ostream& doPrint(std::ostream& os) const override { return os << inner; }
        std::ostream& doPrintDefaultValue(std::ostream& os) const override { return (inner.getDefaultValue().has_value() ? os << inner.getDefaultValue().value() : os << "(none)"); }

        bool doParse(std::string const& token) override { return inner.parse(token); }
        void doReset() override { inner.reset(); }
    };
};

}  // namespace ArgParse

#endif  // QSYN_ARGPARSE_ARGUMENT_H