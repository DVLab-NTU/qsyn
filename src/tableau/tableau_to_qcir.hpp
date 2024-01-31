/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/qcir.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"

namespace qsyn {

namespace experimental {

qcir::QCir to_qcir(StabilizerTableau clifford);
qcir::QCir to_qcir(PauliRotation const& pauli_rotation);
qcir::QCir to_qcir(StabilizerTableau const& clifford, std::vector<PauliRotation> const& pauli_rotations);

}  // namespace experimental

}  // namespace qsyn
