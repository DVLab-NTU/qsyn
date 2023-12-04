/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau base class and its derived classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>
#include <fmt/format.h>

#include <cstddef>
#include <iostream>
#include <ranges>
#include <sul/dynamic_bitset.hpp>

namespace qsyn {

namespace experimental {

class StabilizerTableau {
public:
    StabilizerTableau() = default;
    StabilizerTableau(size_t n_qubits) : _data(2 * n_qubits + 1, sul::dynamic_bitset<>(2 * n_qubits)) {
        for (size_t i = 0; i < 2 * n_qubits; ++i) {
            _data[i].set(i);
        }
    }

    inline size_t n_qubits() const { return (_data.size() - 1) / 2; }

    inline size_t n_rows() const { return _data.size(); }
    inline size_t n_cols() const { return _data.empty() ? 0 : _data[0].size(); }

    void reset(size_t n_qubits) {
        _data.resize(2 * n_qubits + 1);
        for (size_t i = 0; i < 2 * n_qubits; ++i) {
            _data[i].reset();
            _data[i].set(i);
        }
    }

    inline sul::dynamic_bitset<>& operator[](size_t i) { return _data[i]; }
    inline sul::dynamic_bitset<> const& operator[](size_t i) const { return _data[i]; }

    std::string to_string() const;

    inline sul::dynamic_bitset<>& z_row(size_t i) { return _data[z_row_idx(i)]; }
    inline sul::dynamic_bitset<>& x_row(size_t i) { return _data[x_row_idx(i)]; }
    inline sul::dynamic_bitset<>& r_row() { return _data[r_row_idx()]; }

    inline sul::dynamic_bitset<> const& z_row(size_t i) const { return _data[z_row_idx(i)]; }
    inline sul::dynamic_bitset<> const& x_row(size_t i) const { return _data[x_row_idx(i)]; }
    inline sul::dynamic_bitset<> const& r_row() const { return _data[r_row_idx()]; }

    inline size_t stabilizer_idx(size_t j) const { return j; }
    inline size_t destabilizer_idx(size_t j) const { return j + n_qubits(); }

    StabilizerTableau& h(size_t qubit);
    StabilizerTableau& s(size_t qubit);
    StabilizerTableau& cx(size_t ctrl, size_t targ);

    // may be better if we calculate the result directly instead of using circuit identities
    inline StabilizerTableau& sdg(size_t qubit) { return s(qubit).s(qubit).s(qubit); }
    inline StabilizerTableau& x(size_t qubit) { return h(qubit).z(qubit).h(qubit); }
    inline StabilizerTableau& y(size_t qubit) { return x(qubit).z(qubit); }
    inline StabilizerTableau& z(size_t qubit) { return s(qubit).s(qubit); }
    inline StabilizerTableau& cz(size_t ctrl, size_t targ) { return h(targ).cx(ctrl, targ).h(targ); }

    StabilizerTableau& measure_z(size_t qubit);

    bool operator==(StabilizerTableau const& rhs) const { return _data == rhs._data; }
    bool operator!=(StabilizerTableau const& rhs) const { return !(*this == rhs); }

private:
    std::vector<sul::dynamic_bitset<>> _data;

    inline size_t z_row_idx(size_t i) const { return i; }
    inline size_t x_row_idx(size_t i) const { return i + n_qubits(); }
    inline size_t r_row_idx() const { return n_qubits() * 2; }
};

}  // namespace experimental

}  // namespace qsyn

template <>
struct fmt::formatter<qsyn::experimental::StabilizerTableau> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const qsyn::experimental::StabilizerTableau& tableau, FormatContext& ctx) {
        return format_to(ctx.out(), "{}", tableau.to_string());
    }
};
