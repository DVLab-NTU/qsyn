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
struct QCir2TableauResultType : public PauliProductTrait<QCir2TableauResultType> {
    QCir2TableauResultType(StabilizerTableau const& clifford, std::vector<PauliRotation> const& pauli_rotations)
        : clifford{clifford}, pauli_rotations{pauli_rotations} {}
    QCir2TableauResultType& h(size_t qubit) override;
    QCir2TableauResultType& s(size_t qubit) override;
    QCir2TableauResultType& cx(size_t control, size_t target) override;

    StabilizerTableau clifford;
    std::vector<PauliRotation> pauli_rotations;
};

std::optional<QCir2TableauResultType> to_tableau(qcir::QCir const& qcir);

}  // namespace experimental

}  // namespace qsyn
