/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau base class and its derived classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau.hpp"

namespace qsyn::experimental {

std::string StabilizerTableau::to_string() const {
    std::string ret;
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("S{}  {:+c}\n", i, _rotations[stabilizer_idx(i)]);
    }
    ret += '\n';
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("D{}  {:+c}\n", i, _rotations[destabilizer_idx(i)]);
    }
    return ret;
}

std::string StabilizerTableau::to_bit_string() const {
    std::string ret;
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("S{}  {:b}\n", i, _rotations[stabilizer_idx(i)]);
    }
    ret += '\n';
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("D{}  {:b}\n", i, _rotations[destabilizer_idx(i)]);
    }
    return ret;
}

StabilizerTableau& StabilizerTableau::h(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_rotations, [qubit](PauliProduct& p) { p.h(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::s(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_rotations, [qubit](PauliProduct& p) { p.s(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::cx(size_t ctrl, size_t targ) {
    if (ctrl >= n_qubits() || targ >= n_qubits()) return *this;
    std::ranges::for_each(_rotations, [ctrl, targ](PauliProduct& p) { p.cx(ctrl, targ); });
    return *this;
}
}  // namespace qsyn::experimental
