// SPDX-License-Identifier: MIT
#include "cppgridsynth/quantum_gate.hpp"

#include <sstream>
#include <stdexcept>

namespace cppgridsynth {

// ---- CMatrix ------------------------------------------------------------
CMatrix CMatrix::identity(size_t n) {
    CMatrix m(n, n);
    for (size_t i = 0; i < n; ++i) m(i, i) = MPComplex(1.0);
    return m;
}

CMatrix CMatrix::operator*(const CMatrix& o) const {
    if (cols_ != o.rows_) throw std::invalid_argument("CMatrix::operator*: shape mismatch");
    CMatrix out(rows_, o.cols_);
    for (size_t i = 0; i < rows_; ++i) {
        for (size_t j = 0; j < o.cols_; ++j) {
            MPComplex acc;
            for (size_t k = 0; k < cols_; ++k) {
                acc = acc + (*this)(i, k) * o(k, j);
            }
            out(i, j) = acc;
        }
    }
    return out;
}

// ---- Operation default ---------------------------------------------------
std::string Operation::to_string() const {
    std::ostringstream os;
    os << name_ << "(";
    for (size_t i = 0; i < qubits_.size(); ++i) {
        if (i) os << ", ";
        os << qubits_[i];
    }
    os << ")";
    return os.str();
}

// ---- Concrete gate matrices ---------------------------------------------
CMatrix HGate::matrix() const {
    MPFloat inv_sqrt2 = MPFloat(1) / MPFloat::sqrt2();
    CMatrix m(2, 2);
    m(0, 0) = inv_sqrt2;       m(0, 1) = inv_sqrt2;
    m(1, 0) = inv_sqrt2;       m(1, 1) = -inv_sqrt2;
    return m;
}

CMatrix TGate::matrix() const {
    MPFloat phi = MPFloat::pi() / MPFloat(4);
    CMatrix m(2, 2);
    m(0, 0) = MPComplex(1.0);
    m(1, 1) = MPComplex(cos(phi), sin(phi));
    return m;
}

CMatrix SGate::matrix() const {
    CMatrix m(2, 2);
    m(0, 0) = MPComplex(1.0);
    m(1, 1) = MPComplex(MPFloat(0), MPFloat(1));
    return m;
}

CMatrix SXGate::matrix() const {
    CMatrix m(2, 2);
    m(0, 1) = MPComplex(1.0);
    m(1, 0) = MPComplex(1.0);
    return m;
}

CMatrix WGate::matrix() const {
    MPFloat phi = w_phase();
    CMatrix m(1, 1);
    m(0, 0) = MPComplex(cos(phi), sin(phi));
    return m;
}

CMatrix RzGate::matrix() const { return Rz_matrix(theta_); }
CMatrix RxGate::matrix() const { return Rx_matrix(theta_); }
CMatrix CxGate::matrix() const { return cnot01(); }

// ---- Free helpers --------------------------------------------------------
CMatrix Rz_matrix(const MPFloat& theta) {
    CMatrix m(2, 2);
    MPFloat half = theta / MPFloat(2);
    m(0, 0) = MPComplex(cos(-half), sin(-half));
    m(1, 1) = MPComplex(cos(half), sin(half));
    return m;
}

CMatrix Rx_matrix(const MPFloat& theta) {
    CMatrix m(2, 2);
    MPFloat half = theta / MPFloat(2);
    MPFloat c = cos(half);
    MPFloat s = sin(half);
    m(0, 0) = MPComplex(c, MPFloat(0));
    m(1, 1) = MPComplex(c, MPFloat(0));
    m(0, 1) = MPComplex(MPFloat(0), -s);
    m(1, 0) = MPComplex(MPFloat(0), -s);
    return m;
}

CMatrix cnot01() {
    CMatrix m(4, 4);
    m(0, 0) = MPComplex(1.0);
    m(1, 1) = MPComplex(1.0);
    m(2, 3) = MPComplex(1.0);
    m(3, 2) = MPComplex(1.0);
    return m;
}

CMatrix cnot10() {
    CMatrix m(4, 4);
    m(0, 0) = MPComplex(1.0);
    m(1, 3) = MPComplex(1.0);
    m(2, 2) = MPComplex(1.0);
    m(3, 1) = MPComplex(1.0);
    return m;
}

}  // namespace cppgridsynth
