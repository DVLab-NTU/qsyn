/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the interface of SAT solver ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cadical/cadical.hpp>

#include "./sat_solver.hpp"

namespace dvlab::sat {

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
    int _num_vars            = 0;
};

}  // namespace dvlab::sat
