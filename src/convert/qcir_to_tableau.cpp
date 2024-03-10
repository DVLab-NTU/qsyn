/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_to_tableau.hpp"

#include <gsl/narrow>
#include <iterator>
#include <ranges>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <variant>

#include "qcir/gate_type.hpp"
#include "qcir/qcir_gate.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

namespace qsyn {

namespace experimental {

namespace {

#include <algorithm>

Pauli to_pauli(qcir::GateRotationCategory const& category) {
    switch (category) {
        case qcir::GateRotationCategory::rz:
        case qcir::GateRotationCategory::pz:
            return Pauli::z;
        case qcir::GateRotationCategory::rx:
        case qcir::GateRotationCategory::px:
            return Pauli::x;
        case qcir::GateRotationCategory::ry:
        case qcir::GateRotationCategory::py:
            return Pauli::y;
        default:
            DVLAB_UNREACHABLE("Invalid rotation category");
    }
}

bool is_r_type_rotation(qcir::GateRotationCategory const& category) {
    return (
        category == qcir::GateRotationCategory::rz ||
        category == qcir::GateRotationCategory::rx ||
        category == qcir::GateRotationCategory::ry);
}

template <std::ranges::range R, typename T>
bool contains(R const& r, T const& value) {
    return std::ranges::find(r, value) != r.end();
}

template <std::bidirectional_iterator InputIt>
bool next_combination(const InputIt first, InputIt k, const InputIt last) {
    /* Credits: Mark Nelson http://marknelson.us */
    if ((first == last) || (first == k) || (last == k))
        return false;
    InputIt i1 = first;
    InputIt i2 = last;
    ++i1;
    if (last == i1)
        return false;
    i1 = last;
    --i1;
    i1 = k;
    --i2;
    while (first != i1) {
        if (*--i1 < *i2) {
            InputIt j = k;
            while (!(*i1 < *j)) ++j;
            std::iter_swap(i1, j);
            ++i1;
            ++j;
            i2 = k;
            std::rotate(i1, j, last);
            while (last != j) {
                ++j;
                ++i2;
            }
            std::rotate(k, i2, last);
            return true;
        }
    }
    std::rotate(first, k, last);
    return false;
}

template <std::ranges::bidirectional_range R>
auto next_combination(R& r, size_t const comb_size) {
    return next_combination(r.begin(), dvlab::iterator::next(r.begin(), comb_size), r.end());
}

std::vector<size_t> get_qubit_idx_vec(qcir::QCirGate const& gate) {
    auto ret = gate.get_qubits() | tl::to<std::vector<size_t>>();

    std::ranges::sort(ret);

    return ret;
}

void implement_mcrz(Tableau& tableau, qcir::QCirGate const& gate, dvlab::Phase const& phase) {
    if (std::holds_alternative<StabilizerTableau>(tableau.back())) {
        tableau.push_back(std::vector<PauliRotation>{});
    }
    // guaranteed to be a vector of PauliRotation
    auto& last_rotation_group = std::get<std::vector<PauliRotation>>(tableau.back());

    auto const targ = gsl::narrow<size_t>(gate.get_qubit(gate.get_num_qubits() - 1));
    for (auto const comb_size : std::views::iota(0ul, gate.get_num_qubits())) {
        bool const is_neg  = comb_size % 2;
        auto qubit_idx_vec = get_qubit_idx_vec(gate);
        do {  // NOLINT(cppcoreguidelines-avoid-do-while)
            auto const pauli_range =
                std::views::iota(0ul, tableau.n_qubits()) |
                std::views::transform([&qubit_idx_vec, &comb_size, targ](auto i) -> Pauli {
                    // if i is in qubit_idx_range, return Z, otherwise I
                    return (i == targ) || contains(qubit_idx_vec | std::views::take(comb_size), i) ? Pauli::z : Pauli::i;
                }) |
                tl::to<std::vector>();
            last_rotation_group.push_back(PauliRotation(pauli_range.begin(), pauli_range.end(), is_neg ? -phase : phase));
        } while (next_combination(qubit_idx_vec, comb_size));
    }
}

void implement_mcpz(Tableau& tableau, qcir::QCirGate const& gate, dvlab::Phase const& phase) {
    if (std::holds_alternative<StabilizerTableau>(tableau.back())) {
        tableau.push_back(std::vector<PauliRotation>{});
    }
    // guaranteed to be a vector of PauliRotation
    auto& last_rotation_group = std::get<std::vector<PauliRotation>>(tableau.back());

    for (auto const comb_size : std::views::iota(1ul, gate.get_num_qubits() + 1)) {
        bool const is_neg  = (comb_size - 1) % 2;
        auto qubit_idx_vec = get_qubit_idx_vec(gate);
        do {  // NOLINT(cppcoreguidelines-avoid-do-while)
            auto const pauli_range =
                std::views::iota(0ul, tableau.n_qubits()) |
                std::views::transform([&qubit_idx_vec, &comb_size](auto i) -> Pauli {
                    return contains(qubit_idx_vec | std::views::take(comb_size), i) ? Pauli::z : Pauli::i;
                }) |
                tl::to<std::vector>();
            last_rotation_group.push_back(PauliRotation(pauli_range.begin(), pauli_range.end(), is_neg ? -phase : phase));
        } while (next_combination(qubit_idx_vec, comb_size));
    }
}

void implement_rotation_gate(Tableau& tableau, qcir::QCirGate const& gate) {
    auto const pauli = to_pauli(gate.get_rotation_category());

    auto const targ = gsl::narrow<size_t>(gate.get_qubit(gate.get_num_qubits() - 1));
    // convert rotation plane first
    if (pauli == Pauli::x) {
        tableau.h(targ);
    } else if (pauli == Pauli::y) {
        tableau.v(targ);
    }

    dvlab::Phase const phase =
        gate.get_phase() *
        dvlab::Rational(1, static_cast<int>(std::pow(2, gsl::narrow<double>(gate.get_num_qubits()) - 1)));
    // implement rotation in Z plane
    if (is_r_type_rotation(gate.get_rotation_category())) {
        implement_mcrz(tableau, gate, phase);
    } else {
        implement_mcpz(tableau, gate, phase);
    }

    // restore rotation plane
    if (pauli == Pauli::x) {
        tableau.h(targ);
    } else if (pauli == Pauli::y) {
        tableau.vdg(targ);
    }
}

}  // namespace

std::optional<Tableau> to_tableau(qcir::QCir const& qcir) {
    // check if all gates are Clifford
    auto const is_allowed_clifford = [](qcir::QCirGate const& gate) {
        return gate.get_type_str() == "h" ||
               gate.get_type_str() == "s" ||
               gate.get_type_str() == "sdg" ||
               gate.get_type_str() == "v" ||
               gate.get_type_str() == "vdg" ||
               gate.get_type_str() == "x" ||
               gate.get_type_str() == "y" ||
               gate.get_type_str() == "z" ||
               gate.get_type_str() == "cx" ||
               gate.get_type_str() == "cz" ||
               gate.get_type_str() == "swap" ||
               gate.get_type_str() == "ecr";
    };

    for (auto const& gate : qcir.get_gates()) {
        if (!is_allowed_clifford(*gate)) {
            spdlog::error("Gate ID {} of type {} is not allowed in at the moment!!", gate->get_id(), gate->get_type_str());
            return std::nullopt;
        }
    }
    Tableau result{qcir.get_num_qubits()};

    for (auto const& gate : qcir.get_gates()) {
        if (gate->get_type_str() == "h") {
            result.h(gate->get_qubit(0));
        } else if (gate->get_type_str() == "s") {
            result.s(gate->get_qubit(0));
        } else if (gate->get_type_str() == "sdg") {
            result.sdg(gate->get_qubit(0));
        } else if (gate->get_type_str() == "v") {
            result.v(gate->get_qubit(0));
        } else if (gate->get_type_str() == "vdg") {
            result.vdg(gate->get_qubit(0));
        } else if (gate->get_type_str() == "x") {
            result.x(gate->get_qubit(0));
        } else if (gate->get_type_str() == "y") {
            result.y(gate->get_qubit(0));
        } else if (gate->get_type_str() == "z") {
            result.z(gate->get_qubit(0));
        } else if (gate->get_type_str() == "cx") {
            result.cx(gate->get_qubit(0), gate->get_qubit(1));
        } else if (gate->get_type_str() == "cz") {
            result.cz(gate->get_qubit(0), gate->get_qubit(1));
        } else if (gate->get_type_str() == "swap") {
            result.swap(gate->get_qubit(0), gate->get_qubit(1));
        } else if (gate->get_type_str() == "ecr") {
            result.ecr(gate->get_qubit(0), gate->get_qubit(1));
        } else {
            implement_rotation_gate(result, *gate);
        }
    }

    return result;
}

}  // namespace experimental

}  // namespace qsyn
