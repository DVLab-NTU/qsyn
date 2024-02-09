/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <bits/iterator_concepts.h>
#include <fmt/core.h>

#include <csignal>
#include <initializer_list>
#include <ranges>
#include <sul/dynamic_bitset.hpp>
#include <tl/zip.hpp>

#include "util/phase.hpp"

namespace qsyn {

namespace experimental {

enum class Pauli {
    I,
    X,
    Y,
    Z
};

uint8_t power_of_i(Pauli a, Pauli b);

/**
 * @brief Traits for Pauli Product-like classes. Such classes should implement the h, s, cx methods.
          The trait will generate the sdg, v, vdg, x, y, z, cz methods.
 * @tparam T should be the derived class.
 */
template <typename T>
class PauliProductTrait {
public:
    virtual ~PauliProductTrait() = default;

    virtual T& h(size_t qubit)                   = 0;
    virtual T& s(size_t qubit)                   = 0;
    virtual T& cx(size_t control, size_t target) = 0;

    inline T& sdg(size_t qubit) { return s(qubit).s(qubit).s(qubit); }
    inline T& v(size_t qubit) { return h(qubit).s(qubit).h(qubit); }
    inline T& vdg(size_t qubit) { return h(qubit).sdg(qubit).h(qubit); }

    inline T& x(size_t qubit) { return h(qubit).z(qubit).h(qubit); }
    inline T& y(size_t qubit) { return x(qubit).z(qubit); }
    inline T& z(size_t qubit) { return s(qubit).s(qubit); }
    inline T& cz(size_t control, size_t target) { return h(target).cx(control, target).h(target); }
    inline T& swap(size_t qubit1, size_t qubit2) { return cx(qubit1, qubit2).cx(qubit2, qubit1).cx(qubit1, qubit2); }
};

enum class CliffordOperatorType {
    h,
    s,
    cx,
    sdg,
    v,
    vdg,
    x,
    y,
    z,
    cz,
    swap
};

inline CliffordOperatorType adjoint(CliffordOperatorType const& op) {
    using COT = CliffordOperatorType;
    switch (op) {
        case COT::s:
            return COT::sdg;
        case COT::sdg:
            return COT::s;
        case COT::v:
            return COT::vdg;
        case COT::vdg:
            return COT::v;
        default:
            return op;
    }
}

using CliffordOperator = std::pair<CliffordOperatorType, std::array<size_t, 2>>;

class PauliProduct : public PauliProductTrait<PauliProduct> {
public:
    PauliProduct(std::initializer_list<Pauli> const& pauli_list, bool is_neg);
    PauliProduct(std::string_view pauli_str);

    template <std::input_iterator I, std::sentinel_for<I> S>
    PauliProduct(I first, S last, bool is_neg) : _bitset(2 * std::ranges::distance(first, last) + 1) {
        size_t i = 0;
        for (auto it = first; it != last; ++it) {
            set_pauli_type(i++, *it);
        }
        _bitset[r_idx()] = is_neg;
    }

    template <std::ranges::range R>
    PauliProduct(R const& r, bool is_neg) : PauliProduct(std::ranges::begin(r), std::ranges::end(r), is_neg) {}

    ~PauliProduct() override = default;

    inline size_t n_qubits() const { return (_bitset.size() - 1) / 2; }
    inline Pauli get_pauli_type(size_t i) const {
        return is_z_set(i) ? (is_x_set(i) ? Pauli::Y : Pauli::Z)
                           : (is_x_set(i) ? Pauli::X : Pauli::I);
    }

    inline bool is_i(size_t i) const { return !is_z_set(i) && !is_x_set(i); }
    inline bool is_x(size_t i) const { return !is_z_set(i) && is_x_set(i); }
    inline bool is_y(size_t i) const { return is_z_set(i) && is_x_set(i); }
    inline bool is_z(size_t i) const { return is_z_set(i) && !is_x_set(i); }

    inline bool is_neg() const { return _bitset[r_idx()]; }

    PauliProduct& operator*=(PauliProduct const& rhs);

    friend PauliProduct operator*(PauliProduct lhs, PauliProduct const& rhs) {
        lhs *= rhs;
        return lhs;
    }

    inline bool operator==(PauliProduct const& rhs) const { return _bitset == rhs._bitset; }
    inline bool operator!=(PauliProduct const& rhs) const { return _bitset != rhs._bitset; }

    std::string to_string(char signness = '-') const;
    std::string to_bit_string() const;

    PauliProduct& h(size_t qubit) override;
    PauliProduct& s(size_t qubit) override;
    PauliProduct& cx(size_t control, size_t target) override;

    inline PauliProduct& negate() {
        _bitset.flip(r_idx());
        return *this;
    }

    bool is_commutative(PauliProduct const& rhs) const;

    inline void set_pauli_type(size_t i, Pauli type) {
        switch (type) {
            case Pauli::I:
                _bitset[z_idx(i)] = false;
                _bitset[x_idx(i)] = false;
                break;
            case Pauli::X:
                _bitset[z_idx(i)] = false;
                _bitset[x_idx(i)] = true;
                break;
            case Pauli::Y:
                _bitset[z_idx(i)] = true;
                _bitset[x_idx(i)] = true;
                break;
            case Pauli::Z:
                _bitset[z_idx(i)] = true;
                _bitset[x_idx(i)] = false;
                break;
        }
    }

    inline bool is_z_set(size_t i) const { return _bitset[z_idx(i)] == true; }
    inline bool is_x_set(size_t i) const { return _bitset[x_idx(i)] == true; }

private:
    sul::dynamic_bitset<> _bitset;

    inline size_t z_idx(size_t i) const { return i; }
    inline size_t x_idx(size_t i) const { return i + n_qubits(); }
    inline size_t r_idx() const { return n_qubits() * 2; }
};

inline bool is_commutative(PauliProduct const& lhs, PauliProduct const& rhs) {
    return lhs.is_commutative(rhs);
}

class PauliRotation : public PauliProductTrait<PauliRotation> {
public:
    PauliRotation(std::initializer_list<Pauli> const& pauli_list, dvlab::Phase const& phase);
    PauliRotation(std::string_view pauli_str, dvlab::Phase const& phase);

    template <std::input_iterator I, std::sentinel_for<I> S>
    PauliRotation(I first, S last, dvlab::Phase const& phase) : _pauli_product(first, last, false), _phase(phase) {
        normalize();
    }

    template <std::ranges::range R>
    PauliRotation(R const& r, dvlab::Phase const& phase) : PauliRotation(std::ranges::begin(r), std::ranges::end(r), phase) {}

    ~PauliRotation() override = default;

    inline size_t n_qubits() const { return _pauli_product.n_qubits(); }
    inline Pauli get_pauli_type(size_t i) const { return _pauli_product.get_pauli_type(i); }

    inline bool is_i(size_t i) const { return _pauli_product.is_i(i); }
    inline bool is_x(size_t i) const { return _pauli_product.is_x(i); }
    inline bool is_y(size_t i) const { return _pauli_product.is_y(i); }
    inline bool is_z(size_t i) const { return _pauli_product.is_z(i); }

    inline PauliProduct const& pauli_product() const { return _pauli_product; }
    inline dvlab::Phase const& phase() const { return _phase; }
    inline dvlab::Phase& phase() { return _phase; }

    inline bool operator==(PauliRotation const& rhs) const {
        return _pauli_product == rhs._pauli_product && _phase == rhs._phase;
    }
    inline bool operator!=(PauliRotation const& rhs) const { return !(*this == rhs); }

    std::string to_string(char signness = '-') const;
    std::string to_bit_string() const;

    PauliRotation& h(size_t qubit) override;
    PauliRotation& s(size_t qubit) override;
    PauliRotation& cx(size_t control, size_t target) override;

    inline bool is_commutative(PauliRotation const& rhs) const {
        return _pauli_product.is_commutative(rhs._pauli_product);
    }

private:
    PauliProduct _pauli_product;
    dvlab::Phase _phase;

    inline void normalize() {
        if (_pauli_product.is_neg()) {
            _pauli_product.negate();
            _phase *= -1;
        }
    }
};

inline bool is_commutative(PauliRotation const& lhs, PauliRotation const& rhs) {
    return lhs.is_commutative(rhs);
}

}  // namespace experimental

}  // namespace qsyn

template <>
struct fmt::formatter<qsyn::experimental::Pauli> {
    // parse is inherited from formatter<string_view>.
    template <typename FormatContext>
    auto format(qsyn::experimental::Pauli c, FormatContext& ctx) {
        string_view name = "I";
        switch (c) {
            case qsyn::experimental::Pauli::I:
                name = "I";
                break;
            case qsyn::experimental::Pauli::X:
                name = "X";
                break;
            case qsyn::experimental::Pauli::Y:
                name = "Y";
                break;
            case qsyn::experimental::Pauli::Z:
                name = "Z";
                break;
        }
        return fmt::format_to(ctx.out(), "{}", name);
    }
};

template <>
struct fmt::formatter<qsyn::experimental::PauliProduct> {
    char presentation = 'c';
    char signedness   = '-';

    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == '+' || *it == '-' || *it == ' ')) signedness = *it++;
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::PauliProduct const& c, FormatContext& ctx) {
        if (presentation == 'b') {
            return fmt::format_to(ctx.out(), "{}", c.to_bit_string());
        } else {
            return fmt::format_to(ctx.out(), "{}", c.to_string(signedness));
        }
    }
};

template <>
struct fmt::formatter<qsyn::experimental::PauliRotation> {
    char presentation = 'c';
    char signedness   = '-';

    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == '+' || *it == '-' || *it == ' ')) signedness = *it++;
        if (it != end && (*it == 'c' || *it == 'b')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(qsyn::experimental::PauliRotation const& c, FormatContext& ctx) {
        if (presentation == 'b') {
            return fmt::format_to(ctx.out(), "{}", c.to_bit_string());
        } else {
            return fmt::format_to(ctx.out(), "{}", c.to_string(signedness));
        }
    }
};
