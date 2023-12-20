/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define class Phase_Polynomial structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "util/boolean_matrix.hpp"
#include "util/phase.hpp"

namespace dvlab {

class Phase;

}

namespace qsyn::qcir {

class QCir;

}

namespace qsyn::pp {

class Phase_Polynomial {


// using Monomial   = std::pair<dvlab::BooleanMatrix, dvlab::Phase>;
// using Polynomial_terms = std::unordered_map<dvlab::BooleanMatrix::Row, dvlab::Phase>;
  
public:
    

    Phase_Polynomial(){};

    bool calculate_pp(qcir::QCir const& qcir);
    bool insert_phase(size_t, dvlab::Phase);
    void reset();
    void intial_wire(size_t);
    // qcir::QCir* resynthesis(Polynomial*){};

private:
    size_t _qubit_number;
    size_t _ancillae;
    dvlab::BooleanMatrix _pp_terms;
    std::vector<dvlab::Phase> _pp_coeff;
    dvlab::BooleanMatrix _wires;
};

}  // namespace qsyn::pp
