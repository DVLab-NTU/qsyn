/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the interface of SAT solver ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <NamedType/named_type.hpp>
#include <NamedType/underlying_functionalities.hpp>
#include <vector>

namespace dvlab::sat {

// Variable is a positive integer
using Variable = fluent::NamedType<int, struct VariableTag, fluent::Comparable, fluent::Hashable, fluent::PreIncrementable>;

// Literal is a signed integer
// The absolute value of Lit is the corresponding Var
// Negation of Lit is represented by the negative sign
class Literal : public fluent::NamedType<int, struct LiteralTag, fluent::Comparable, fluent::Hashable, fluent::UnarySubtractable> {
private:
    using _Literal = fluent::NamedType<int, struct LiteralTag, fluent::Comparable, fluent::Hashable, fluent::UnarySubtractable>;

public:
    Literal() = default;
    Literal(int value) : _Literal(value) {}
    Literal(Variable var, bool negate = false) : _Literal(negate ? var.get() : -var.get()) {}
    Variable get_variable() const { return Variable(std::abs(get())); }
    bool get_negated() const { return get() < 0; }
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

}  // namespace dvlab::sat
