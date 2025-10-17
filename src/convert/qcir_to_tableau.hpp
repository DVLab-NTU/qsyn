/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "qcir/operation.hpp"
#include "qcir/qcir.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/tableau.hpp"

namespace qsyn {

namespace tableau {

std::optional<Tableau> to_tableau(qcir::QCir const& qcir);

}  // namespace tableau

template <>
bool append_to_tableau(qcir::QCir const& op, tableau::Tableau& tableau, QubitIdList const& qubits);

}  // namespace qsyn
