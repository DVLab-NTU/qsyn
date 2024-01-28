/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "pauli_rotation.hpp"

#include "util/dvlab_string.hpp"

namespace qsyn {

namespace experimental {

uint8_t power_of_i(Pauli a, Pauli b) {
    if (a == Pauli::X && b == Pauli::Y) return 1;
    if (a == Pauli::Y && b == Pauli::Z) return 1;
    if (a == Pauli::Z && b == Pauli::X) return 1;

    if (a == Pauli::X && b == Pauli::Z) return 3;
    if (a == Pauli::Z && b == Pauli::Y) return 3;
    if (a == Pauli::Y && b == Pauli::X) return 3;
    return 0;
}

PauliProduct::PauliProduct(std::initializer_list<Pauli> const& pauli_list, bool is_neg)
    : _bitset(2 * pauli_list.size() + 1, 0) {
    if (is_neg) {
        _bitset.set(2 * pauli_list.size());
    }

    for (size_t i = 0; i < pauli_list.size(); ++i) {
        switch (pauli_list.begin()[i]) {
            case Pauli::I:
                break;
            case Pauli::Z:
                _bitset.set(z_idx(i));
                break;
            case Pauli::Y:
                _bitset.set(z_idx(i));
                _bitset.set(x_idx(i));
                break;
            case Pauli::X:
                _bitset.set(x_idx(i));
                break;
        }
    }
}

PauliProduct::PauliProduct(std::string_view pauli_str) {
    bool is_neg = false;
    if (pauli_str.front() == '+') {
        pauli_str.remove_prefix(1);
    } else if (pauli_str.front() == '-') {
        pauli_str.remove_prefix(1);
        is_neg = true;
    }
    _bitset.resize(2 * pauli_str.size() + 1);
    if (is_neg) {
        _bitset.set(2 * pauli_str.size());
    }
    for (size_t i = 0; i < pauli_str.size(); ++i) {
        switch (dvlab::str::toupper(pauli_str[i])) {
            case 'I':
                break;
            case 'Z':
                _bitset.set(z_idx(i));
                break;
            case 'Y':
                _bitset.set(z_idx(i));
                _bitset.set(x_idx(i));
                break;
            case 'X':
                _bitset.set(x_idx(i));
                break;
        }
    }
}

PauliProduct& PauliProduct::operator*=(PauliProduct const& rhs) {
    assert(n_qubits() == rhs.n_qubits());
    // calculate the sign
    uint8_t power_of_i = 0;
    for (size_t i = 0; i < n_qubits(); ++i) {
        power_of_i += qsyn::experimental::power_of_i(get_pauli_type(i), rhs.get_pauli_type(i));
    }
    assert(power_of_i % 2 == 0);
    if (power_of_i % 4 == 2) {
        _bitset.flip(r_idx());
    }
    _bitset ^= rhs._bitset;
    return *this;
}

std::string PauliProduct::to_string(char signedness) const {
    std::string str;
    if (is_neg()) {
        str += '-';
    } else if (signedness == '+') {
        str += '+';
    } else if (signedness == ' ') {
        str += ' ';
    }

    for (size_t i = 0; i < n_qubits(); ++i) {
        switch (get_pauli_type(i)) {
            case Pauli::I:
                str += 'I';
                break;
            case Pauli::X:
                str += 'X';
                break;
            case Pauli::Y:
                str += 'Y';
                break;
            case Pauli::Z:
                str += 'Z';
                break;
        }
    }
    return str;
}

std::string PauliProduct::to_bit_string() const {
    std::string str;
    for (size_t i = 0; i < n_qubits(); ++i) {
        str += is_z_set(i) ? '1' : '0';
    }
    str += ' ';
    for (size_t i = 0; i < n_qubits(); ++i) {
        str += is_x_set(i) ? '1' : '0';
    }
    str += ' ';
    str += is_neg() ? '1' : '0';
    return str;
}

namespace {

void bitset_swap_two(size_t i, size_t j, sul::dynamic_bitset<>& bitset) {
    assert(i < bitset.size());
    assert(j < bitset.size());
    bitset[i] ^= bitset[j];
    bitset[j] ^= bitset[i];
    bitset[i] ^= bitset[j];
}

}  // namespace

PauliProduct& PauliProduct::h(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    if (is_y(qubit)) {
        _bitset.flip(r_idx());
    }
    bitset_swap_two(z_idx(qubit), x_idx(qubit), _bitset);
    return *this;
}

PauliProduct& PauliProduct::s(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    if (is_y(qubit)) {
        _bitset.flip(r_idx());
    }
    _bitset[z_idx(qubit)] ^= _bitset[x_idx(qubit)];
    return *this;
}

PauliProduct& PauliProduct::cx(size_t control, size_t target) {
    if (control >= n_qubits() || target >= n_qubits()) {
        return *this;
    }
    if (_bitset[x_idx(control)] && _bitset[z_idx(target)] && (_bitset[x_idx(target)] == _bitset[z_idx(control)])) {
        _bitset.flip(r_idx());
    }
    _bitset[x_idx(target)] ^= _bitset[x_idx(control)];
    _bitset[z_idx(control)] ^= _bitset[z_idx(target)];

    return *this;
}

bool PauliProduct::is_commutative(PauliProduct const& rhs) const {
    assert(n_qubits() == rhs.n_qubits());
    bool is_commutative = true;
    for (size_t i = 0; i < n_qubits(); ++i) {
        is_commutative ^= (is_z_set(i) && rhs.is_x_set(i)) ^ (is_x_set(i) && rhs.is_z_set(i));
    }
    return is_commutative;
}

PauliRotation::PauliRotation(std::initializer_list<Pauli> const& pauli_list, dvlab::Phase const& phase)
    : _pauli_product(pauli_list, false), _phase(phase) { normalize(); }

PauliRotation::PauliRotation(std::string_view pauli_str, dvlab::Phase const& phase)
    : _pauli_product(pauli_str), _phase(phase) { normalize(); }

std::string PauliRotation::to_string(char signedness) const {
    return fmt::format("exp(i * {} * {})", _phase.get_print_string(), _pauli_product.to_string(signedness));
}

std::string PauliRotation::to_bit_string() const {
    return fmt::format("{} {}", _pauli_product.to_bit_string().substr(0, 2 * n_qubits() + 1), _phase.get_print_string());
}

PauliRotation& PauliRotation::h(size_t qubit) {
    _pauli_product.h(qubit);
    normalize();
    return *this;
}

PauliRotation& PauliRotation::s(size_t qubit) {
    _pauli_product.s(qubit);
    normalize();
    return *this;
}

PauliRotation& PauliRotation::cx(size_t control, size_t target) {
    _pauli_product.cx(control, target);
    normalize();
    return *this;
}

}  // namespace experimental

}  // namespace qsyn
