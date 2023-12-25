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
    constexpr Variable() : _Variable(0) {}  // for placeholder
    explicit constexpr Variable(int const& value) : _Variable(value) {}
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
    constexpr Literal() : _Literal(0) {}  // for placeholder
    explicit constexpr Literal(int value) : _Literal(value) {}
    constexpr Literal(Variable var) : _Literal(var.get()) {
        if (var.get() <= 0) {
            throw std::invalid_argument(fmt::format("Variable must be positive, but got {}", var.get()));
        }
    }
    explicit constexpr Literal(Variable var, bool negate) : _Literal(negate ? -var.get() : var.get()) {
        if (var.get() <= 0) {
            throw std::invalid_argument(fmt::format("Variable must be positive, but got {}", var.get()));
        }
    }
    [[nodiscard]] Literal constexpr operator~() const {
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

class Solution {
    friend class SatSolver;

public:
    Solution(size_t const& num_vars) : _values(num_vars, false) {}
    bool operator[](Variable const& var) const { return get(var); }
    // return true if the variable is assigned to true
    bool get(Variable const& var) const { return _values[var.get() - 1]; }

    void set(Variable const& var, bool value) { _values[var.get() - 1] = value; }

    size_t size() const { return _values.size(); }

private:
    std::vector<bool> _values;
};

class SatSolver {
public:
    virtual ~SatSolver()                                        = default;
    virtual void reset()                                        = 0;
    virtual void add_clause(std::vector<Literal> const& clause) = 0;
    virtual void assume(Literal lit)                            = 0;
    virtual Result solve()                                      = 0;
    virtual std::optional<Solution> get_solution()              = 0;

    void add_gte_constraint(std::vector<Literal> const& literals, size_t const& k);
    void assume_all(std::vector<Literal> const& literals);
    Variable new_var() { return _next_var++; }

protected:
    Variable _next_var = Variable(1);
};

class CaDiCalSolver : public SatSolver {
public:
    CaDiCalSolver() {}
    ~CaDiCalSolver() override;
    void reset() override;
    void add_clause(std::vector<Literal> const& clause) override;
    void assume(Literal lit) override;
    Result solve() override;
    std::optional<Solution> get_solution() override;

private:
    CaDiCaL::Solver* _solver = new CaDiCaL::Solver();
};

}  // namespace dvlab::sat
