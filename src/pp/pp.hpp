/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define class Phase_Polynomial structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "util/boolean_matrix.hpp"

namespace qsyn {

namespace qcir {
class QCir;
}

namespace pp {

class Phase_Polynomial{
public:
    using Monomial    =  std::pair<dvlab::BooleanMatrix, dvlab::Phase> ;
    using Polynomial  =  std::vector<Monomial>;
    using Wire        =  std::vector<dvlab::BooleanMatrix>;
        
    
    Phase_Polynomial(qcir::QCir*, size_t, std::string);

    // Polynomial calculate_pp(qcir::QCir*){};
    // qcir::QCir* resynthesis(Polynomial*){};

};

}




} // namespace qsyn::qcir