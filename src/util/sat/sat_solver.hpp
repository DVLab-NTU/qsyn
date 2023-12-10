/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the interface of SAT solver ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once
#include <vector>

namespace dvlab::sat {

// Var is a positive integer
typedef int Variable;
// Lit is a signed integer
// The absolute value of Lit is the corresponding Var
// Negation of Lit is represented by the negative sign
typedef int Literal;

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
