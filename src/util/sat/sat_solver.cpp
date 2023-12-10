/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the interface of SAT solver ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./sat_solver.hpp"

namespace dvlab::sat {

void SatSolver::assume_all(std::vector<Literal> const& literals) {
    for (auto const& lit : literals) {
        assume(lit);
    }
}

}  // namespace dvlab::sat
