/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <set>

#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn {

namespace qcir {
class QCir;
}
namespace tensor {

class QTensor;
}

namespace decomposer {

template <typename U>

class Decomposer {
public:

    Decomposer(int qreg);
    qcir::QCir* get_qcir() { return _quantum_circuit; }
    int get_qreg() { return qreg; }
    void decompose();


private:

    qcir::QCir* _quantum_circuit;
    tensor::QTensor<U> _matrix;
    int qreg;

    
};

}  // namespace decomposer

}  // namespace qsyn
