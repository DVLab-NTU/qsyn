// SPDX-License-Identifier: MIT
//
// DOmegaUnitary: representation of a 2x2 unitary as a triple
// (z, w, n) where the matrix equals
//
//          [   z         -w*.ω^n  ]
//          [   w          z*.ω^n  ]   (z, w ∈ D[ω],  n ∈ {0,...,7})
//
// This mirrors pygridsynth's DOmegaUnitary class.
#pragma once

#include <array>
#include <optional>
#include <string>

#include "cppgridsynth/quantum_circuit.hpp"
#include "cppgridsynth/ring.hpp"

namespace cppgridsynth {

class DOmegaUnitary {
public:
    DOmegaUnitary(DOmega z, DOmega w, long n, std::optional<long> k = std::nullopt);

    const DOmega& z() const noexcept { return z_; }
    const DOmega& w() const noexcept { return w_; }
    long n() const noexcept { return n_; }
    long k() const noexcept { return w_.k(); }

    // ---- Multiplications-from-the-left by Clifford+T generators ----------
    DOmegaUnitary mul_by_T_from_left() const;
    DOmegaUnitary mul_by_T_inv_from_left() const;
    DOmegaUnitary mul_by_T_power_from_left(long m) const;
    DOmegaUnitary mul_by_S_from_left() const;
    DOmegaUnitary mul_by_S_power_from_left(long m) const;
    DOmegaUnitary mul_by_H_from_left() const;
    DOmegaUnitary mul_by_H_and_T_power_from_left(long m) const;
    DOmegaUnitary mul_by_X_from_left() const;
    DOmegaUnitary mul_by_W_from_left() const;
    DOmegaUnitary mul_by_W_power_from_left(long m) const;

    DOmegaUnitary renew_denomexp(long new_k) const {
        return DOmegaUnitary(z_, w_, n_, new_k);
    }
    DOmegaUnitary reduce_denomexp() const;

    bool operator==(const DOmegaUnitary& o) const {
        return z_ == o.z_ && w_ == o.w_ && n_ == o.n_;
    }
    bool operator!=(const DOmegaUnitary& o) const { return !(*this == o); }

    // Identity unitary.
    static DOmegaUnitary identity();
    static DOmegaUnitary from_gates(const std::string& gates);

    // 2x2 matrix with DOmega entries.
    std::array<std::array<DOmega, 2>, 2> to_matrix() const;
    std::string to_string() const;

private:
    DOmega z_;
    DOmega w_;
    long n_;
};

}  // namespace cppgridsynth
