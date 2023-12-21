/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the interface of SAT solver ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/format.h>

#include <NamedType/named_type.hpp>
#include <NamedType/underlying_functionalities.hpp>
#include <cadical/cadical.hpp>
#include <vector>

namespace dvlab::sat {

// Variable is a positive integer
class Variable : public fluent::NamedType<int, struct VariableTag, fluent::Comparable, fluent::Hashable, fluent::PostIncrementable> {
private:
    using _Variable = fluent::NamedType<int, struct VariableTag, fluent::Comparable, fluent::Hashable, fluent::PostIncrementable>;

public:
    explicit constexpr Variable(int const& value) : _Variable(value) {
        if (value <= 0) {
            throw std::invalid_argument(fmt::format("Variable must be positive, but got {}", value));
        }
    }
    constexpr Variable& operator++(int) {
        _Variable::operator++(1);
        return *this;
    }
};

// Literal is a signed integer
// The absolute value of Lit is the corresponding Var
// Negation of Lit is represented by the negative sign
class Literal : public fluent::NamedType<int, struct LiteralTag, fluent::Comparable, fluent::Hashable> {
private:
    using _Literal = fluent::NamedType<int, struct LiteralTag, fluent::Comparable, fluent::Hashable>;

public:
    constexpr Literal(int value) : _Literal(value) {}
    constexpr Literal(Variable var, bool negate = false) : _Literal(negate ? -var.get() : var.get()) {}
    [[nodiscard]] Literal constexpr operator-() const {
        return Literal(-get());
    }
    constexpr Variable get_variable() const { return Variable(std::abs(get())); }
    constexpr bool is_negated() const { return get() < 0; }
};

enum class Result {
    SAT,
    UNSAT,
    UNKNOWN
};

class SatSolver {
public:
    virtual ~SatSolver()                                        = default;
    virtual void reset()                                        = 0;
    virtual Variable new_var()                                  = 0;
    virtual void add_clause(std::vector<Literal> const& clause) = 0;
    virtual void assume(Literal lit)                            = 0;
    virtual Result solve()                                      = 0;

    void assume_all(std::vector<Literal> const& literals);
};

class CaDiCalSolver : public SatSolver {
public:
    CaDiCalSolver() {}
    ~CaDiCalSolver() override;
    void reset() override;
    Variable new_var() override;
    void add_clause(std::vector<Literal> const& clause) override;
    void assume(Literal lit) override;
    Result solve() override;

private:
    CaDiCaL::Solver* _solver = new CaDiCaL::Solver();
    Variable _next_var       = Variable(1);
};

}  // namespace dvlab::sat
