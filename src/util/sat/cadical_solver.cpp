/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the interface of SAT solver ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./cadical_solver.hpp"

namespace dvlab::sat {

CaDiCalSolver::~CaDiCalSolver() { delete _solver; }

void CaDiCalSolver::reset() {
    delete _solver;
    _solver   = new CaDiCaL::Solver();
    _num_vars = Variable(0);
}

Variable CaDiCalSolver::new_var() { return ++_num_vars; }

void CaDiCalSolver::add_clause(std::vector<Literal> const& clause) {
    for (auto const& lit : clause) {
        _solver->add(lit.get());
    }
    _solver->add(0);
}

void CaDiCalSolver::assume(Literal lit) {
    _solver->assume(lit.get());
}

Result CaDiCalSolver::solve() {
    switch (_solver->solve()) {
        case 10:
            return Result::SAT;
        case 20:
            return Result::UNSAT;
        case 0:
        default:
            return Result::UNKNOWN;
    }
}

}  // namespace dvlab::sat