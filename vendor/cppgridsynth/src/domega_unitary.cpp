// SPDX-License-Identifier: MIT
#include "cppgridsynth/domega_unitary.hpp"

#include <sstream>
#include <stdexcept>

namespace cppgridsynth {

namespace {
inline long norm_n(long n) { return ((n % 8) + 8) % 8; }
}

DOmegaUnitary::DOmegaUnitary(DOmega z, DOmega w, long n, std::optional<long> k)
    : z_(std::move(z)), w_(std::move(w)), n_(norm_n(n)) {
    if (k.has_value()) {
        z_ = z_.renew_denomexp(*k);
        w_ = w_.renew_denomexp(*k);
    } else if (z_.k() > w_.k()) {
        w_ = w_.renew_denomexp(z_.k());
    } else if (z_.k() < w_.k()) {
        z_ = z_.renew_denomexp(w_.k());
    }
}

DOmegaUnitary DOmegaUnitary::identity() {
    return DOmegaUnitary(DOmega::from_int(1), DOmega::from_int(0), 0);
}

DOmegaUnitary DOmegaUnitary::mul_by_T_from_left() const {
    return DOmegaUnitary(z_, w_.mul_by_omega(), n_ + 1);
}
DOmegaUnitary DOmegaUnitary::mul_by_T_inv_from_left() const {
    return DOmegaUnitary(z_, w_.mul_by_omega_inv(), n_ - 1);
}
DOmegaUnitary DOmegaUnitary::mul_by_T_power_from_left(long m) const {
    m = norm_n(m);
    return DOmegaUnitary(z_, w_.mul_by_omega_power(m), n_ + m);
}
DOmegaUnitary DOmegaUnitary::mul_by_S_from_left() const {
    return DOmegaUnitary(z_, w_.mul_by_omega_power(2), n_ + 2);
}
DOmegaUnitary DOmegaUnitary::mul_by_S_power_from_left(long m) const {
    m = ((m % 4) + 4) % 4;
    return DOmegaUnitary(z_, w_.mul_by_omega_power(m << 1), n_ + (m << 1));
}
DOmegaUnitary DOmegaUnitary::mul_by_H_from_left() const {
    DOmega new_z = (z_ + w_).mul_by_inv_sqrt2();
    DOmega new_w = (z_ - w_).mul_by_inv_sqrt2();
    return DOmegaUnitary(new_z, new_w, n_ + 4);
}
DOmegaUnitary DOmegaUnitary::mul_by_H_and_T_power_from_left(long m) const {
    return mul_by_T_power_from_left(m).mul_by_H_from_left();
}
DOmegaUnitary DOmegaUnitary::mul_by_X_from_left() const {
    return DOmegaUnitary(w_, z_, n_ + 4);
}
DOmegaUnitary DOmegaUnitary::mul_by_W_from_left() const {
    return DOmegaUnitary(z_.mul_by_omega(), w_.mul_by_omega(), n_ + 2);
}
DOmegaUnitary DOmegaUnitary::mul_by_W_power_from_left(long m) const {
    m = norm_n(m);
    return DOmegaUnitary(z_.mul_by_omega_power(m), w_.mul_by_omega_power(m), n_ + (m << 1));
}

DOmegaUnitary DOmegaUnitary::reduce_denomexp() const {
    return DOmegaUnitary(z_.reduce_denomexp(), w_.reduce_denomexp(), n_);
}

DOmegaUnitary DOmegaUnitary::from_gates(const std::string& gates) {
    DOmegaUnitary u = identity();
    for (auto it = gates.rbegin(); it != gates.rend(); ++it) {
        char g = *it;
        switch (g) {
            case 'H': u = u.renew_denomexp(u.k() + 1).mul_by_H_from_left(); break;
            case 'T': u = u.mul_by_T_from_left(); break;
            case 'S': u = u.mul_by_S_from_left(); break;
            case 'X': u = u.mul_by_X_from_left(); break;
            case 'W': u = u.mul_by_W_from_left(); break;
            default:
                throw std::invalid_argument(std::string("DOmegaUnitary::from_gates: unknown '") + g + "'");
        }
    }
    return u.reduce_denomexp();
}

std::array<std::array<DOmega, 2>, 2> DOmegaUnitary::to_matrix() const {
    DOmega minus_w_conj_omega_n = -w_.conj().mul_by_omega_power(n_);
    DOmega z_conj_omega_n = z_.conj().mul_by_omega_power(n_);
    return {{ {{z_, minus_w_conj_omega_n}}, {{w_, z_conj_omega_n}} }};
}

std::string DOmegaUnitary::to_string() const {
    std::ostringstream os;
    auto m = to_matrix();
    os << "[[" << m[0][0].to_string() << ", " << m[0][1].to_string() << "], "
       << "[" << m[1][0].to_string() << ", " << m[1][1].to_string() << "]]";
    return os.str();
}

}  // namespace cppgridsynth
