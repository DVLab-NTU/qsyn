/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau base class and its derived classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau.hpp"

namespace qsyn::experimental {

std::string StabilizerTableau::to_string() const {
    std::string result;
    for (size_t i = 0; i < n_qubits(); ++i) {
        for (size_t j = 0; j < n_qubits(); ++j) {
            result += fmt::format("{}", z_row(i)[stabilizer_idx(j)] ? "1" : "0");
        }
        result += ' ';
        for (size_t j = 0; j < n_qubits(); ++j) {
            result += fmt::format("{}", z_row(i)[destabilizer_idx(j)] ? "1" : "0");
        }
        result += '\n';
    }
    result += '\n';
    for (size_t i = 0; i < n_qubits(); ++i) {
        for (size_t j = 0; j < n_qubits(); ++j) {
            result += fmt::format("{}", x_row(i)[stabilizer_idx(j)] ? "1" : "0");
        }
        result += ' ';
        for (size_t j = 0; j < n_qubits(); ++j) {
            result += fmt::format("{}", x_row(i)[destabilizer_idx(j)] ? "1" : "0");
        }
        result += '\n';
    }
    result += '\n';
    for (size_t j = 0; j < n_qubits(); ++j) {
        result += fmt::format("{}", r_row()[stabilizer_idx(j)] ? "1" : "0");
    }
    result += ' ';
    for (size_t j = 0; j < n_qubits(); ++j) {
        result += fmt::format("{}", r_row()[destabilizer_idx(j)] ? "1" : "0");
    }
    result += '\n';
    return result;
}

StabilizerTableau& StabilizerTableau::h(size_t qubit) {
    if (qubit >= n_qubits()) return *this;

    r_row() ^= (x_row(qubit) & z_row(qubit));
    std::swap(z_row(qubit), x_row(qubit));

    return *this;
}

StabilizerTableau& StabilizerTableau::s(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    // order matters!!
    r_row() ^= (x_row(qubit) & z_row(qubit));
    z_row(qubit) ^= x_row(qubit);

    return *this;
}

StabilizerTableau& StabilizerTableau::cx(size_t ctrl, size_t targ) {
    if (ctrl >= n_qubits() || targ >= n_qubits()) {
        return *this;
    }
    // order matters!!
    r_row() ^= (x_row(ctrl) & z_row(targ) & (x_row(targ) ^ ~z_row(targ)));
    x_row(targ) ^= x_row(ctrl);
    z_row(ctrl) ^= z_row(targ);

    return *this;
}
}  // namespace qsyn::experimental
