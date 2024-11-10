/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_to_tableau.hpp"

#include <gsl/narrow>
#include <ranges>
#include <span>
#include <tl/adjacent.hpp>
#include <tl/generator.hpp>
#include <tl/to.hpp>
#include <unordered_map>
#include <variant>

#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tableau/tableau.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

bool stop_requested();

namespace qsyn {

namespace experimental {

namespace {

#include <algorithm>

std::vector<size_t> get_qubit_idx_vec(QubitIdList const& qubits) {
    auto ret = qubits | tl::to<std::vector<size_t>>();

    std::ranges::sort(ret);

    return ret;
}

[[nodiscard]] bool implement_mcr(Tableau& tableau, QubitIdList const& qubits, dvlab::Phase const& ph, Pauli pauli) {
    if (std::holds_alternative<StabilizerTableau>(tableau.back())) {
        tableau.push_back(std::vector<PauliRotation>{});
    }

    dvlab::Phase const phase =
        ph *
        dvlab::Rational(1, static_cast<int>(std::pow(2, gsl::narrow<double>(qubits.size()) - 1)));

    auto const targ = qubits.back();
    // convert rotation plane first
    if (pauli == Pauli::x) {
        tableau.h(targ);
    } else if (pauli == Pauli::y) {
        tableau.v(targ);
    }
    // guaranteed to be a vector of PauliRotation
    auto& last_rotation_group = std::get<std::vector<PauliRotation>>(tableau.back());

    for (auto const comb_size : std::views::iota(0ul, qubits.size())) {
        bool const is_neg  = comb_size % 2;
        auto qubit_idx_vec = get_qubit_idx_vec(qubits);
        for (auto qubit_idx_vec : dvlab::combinations(get_qubit_idx_vec(qubits), comb_size)) {
            if (stop_requested()) {
                return false;
            }
            auto const pauli_range =
                std::views::iota(0ul, tableau.n_qubits()) |
                std::views::transform([&qubit_idx_vec, targ](auto i) -> Pauli {
                    // if i is in qubit_idx_range, return Z, otherwise I
                    return (i == targ) || dvlab::contains(qubit_idx_vec, i) ? Pauli::z : Pauli::i;
                }) |
                tl::to<std::vector>();
            last_rotation_group.push_back(PauliRotation(pauli_range.begin(), pauli_range.end(), is_neg ? -phase : phase));
        }
    }
    // restore rotation plane
    if (pauli == Pauli::x) {
        tableau.h(targ);
    } else if (pauli == Pauli::y) {
        tableau.vdg(targ);
    }

    return true;
}

[[nodiscard]] bool implement_mcp(Tableau& tableau, QubitIdList const& qubits, dvlab::Phase const& ph, Pauli pauli) {
    if (std::holds_alternative<StabilizerTableau>(tableau.back())) {
        tableau.push_back(std::vector<PauliRotation>{});
    }

    dvlab::Phase const phase =
        ph *
        dvlab::Rational(1, static_cast<int>(std::pow(2, gsl::narrow<double>(qubits.size()) - 1)));

    auto const targ = qubits.back();
    // convert rotation plane first
    if (pauli == Pauli::x) {
        tableau.h(targ);
    } else if (pauli == Pauli::y) {
        tableau.v(targ);
    }
    // guaranteed to be a vector of PauliRotation
    auto& last_rotation_group = std::get<std::vector<PauliRotation>>(tableau.back());

    for (auto const comb_size : std::views::iota(1ul, qubits.size() + 1)) {
        bool const is_neg  = (comb_size - 1) % 2;
        auto qubit_idx_vec = get_qubit_idx_vec(qubits);
        for (auto qubit_idx_vec : dvlab::combinations(get_qubit_idx_vec(qubits), comb_size)) {
            if (stop_requested()) {
                return false;
            }
            auto const pauli_range =
                std::views::iota(0ul, tableau.n_qubits()) |
                std::views::transform([&qubit_idx_vec](auto i) -> Pauli {
                    return dvlab::contains(qubit_idx_vec, i) ? Pauli::z : Pauli::i;
                }) |
                tl::to<std::vector>();
            last_rotation_group.push_back(PauliRotation(pauli_range.begin(), pauli_range.end(), is_neg ? -phase : phase));
        }
    }
    // restore rotation plane
    if (pauli == Pauli::x) {
        tableau.h(targ);
    } else if (pauli == Pauli::y) {
        tableau.vdg(targ);
    }

    return true;
}

}  // namespace

}  // namespace experimental

template <>
bool append_to_tableau(qcir::IdGate const& /* op*/, experimental::Tableau& /* tableau */, QubitIdList const& /* qubits */) {
    return true;
}

template <>
bool append_to_tableau(qcir::HGate const& /* op */, experimental::Tableau& tableau, QubitIdList const& qubits) {
    tableau.h(qubits[0]);
    return true;
}

template <>
bool append_to_tableau(qcir::SwapGate const& /* op */, experimental::Tableau& tableau, QubitIdList const& qubits) {
    tableau.swap(qubits[0], qubits[1]);
    return true;
}

template <>
bool append_to_tableau(qcir::ECRGate const& /* op */, experimental::Tableau& tableau, QubitIdList const& qubits) {
    tableau.ecr(qubits[0], qubits[1]);
    return true;
}

template <>
bool append_to_tableau(qcir::PZGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
    if (op.get_phase() == dvlab::Phase(1)) {
        tableau.z(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(1, 2)) {
        tableau.s(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(-1, 2)) {
        tableau.sdg(qubits[0]);
    } else {
        return experimental::implement_mcp(tableau, qubits, op.get_phase(), experimental::Pauli::z);
    }

    return true;
}

template <>
bool append_to_tableau(qcir::PXGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
    if (op.get_phase() == dvlab::Phase(1)) {
        tableau.x(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(1, 2)) {
        tableau.v(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(-1, 2)) {
        tableau.vdg(qubits[0]);
    } else {
        return experimental::implement_mcp(tableau, qubits, op.get_phase(), experimental::Pauli::x);
    }

    return true;
}

template <>
bool append_to_tableau(qcir::PYGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
    if (op.get_phase() == dvlab::Phase(1)) {
        tableau.y(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(1, 2)) {
        tableau.sdg(qubits[0]);
        tableau.v(qubits[0]);
        tableau.s(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(-1, 2)) {
        tableau.sdg(qubits[0]);
        tableau.vdg(qubits[0]);
        tableau.s(qubits[0]);
    } else {
        return experimental::implement_mcp(tableau, qubits, op.get_phase(), experimental::Pauli::y);
    }

    return true;
}

template <>
bool append_to_tableau(qcir::RZGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
    if (op.get_phase() == dvlab::Phase(1)) {
        tableau.z(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(1, 2)) {
        tableau.s(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(-1, 2)) {
        tableau.sdg(qubits[0]);
    } else {
        return experimental::implement_mcr(tableau, qubits, op.get_phase(), experimental::Pauli::z);
    }
    return true;
}

template <>
bool append_to_tableau(qcir::RXGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
    if (op.get_phase() == dvlab::Phase(1)) {
        tableau.x(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(1, 2)) {
        tableau.v(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(-1, 2)) {
        tableau.vdg(qubits[0]);
    } else {
        return experimental::implement_mcr(tableau, qubits, op.get_phase(), experimental::Pauli::x);
    }
    return true;
}

template <>
bool append_to_tableau(qcir::RYGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
    if (op.get_phase() == dvlab::Phase(1)) {
        tableau.y(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(1, 2)) {
        tableau.sdg(qubits[0]);
        tableau.v(qubits[0]);
        tableau.s(qubits[0]);
    } else if (op.get_phase() == dvlab::Phase(-1, 2)) {
        tableau.sdg(qubits[0]);
        tableau.vdg(qubits[0]);
        tableau.s(qubits[0]);
    } else {
        return experimental::implement_mcr(tableau, qubits, op.get_phase(), experimental::Pauli::y);
    }
    return true;
}

template <>
bool append_to_tableau(qcir::ControlGate const& op, experimental::Tableau& tableau, QubitIdList const& qubits) {
    if (auto target_op = op.get_target_operation().get_underlying_if<qcir::PXGate>()) {
        if (op.get_num_qubits() == 2 && target_op->get_phase() == dvlab::Phase(1)) {
            tableau.cx(qubits[0], qubits[1]);
            return true;
        } else {
            return experimental::implement_mcp(tableau, qubits, target_op->get_phase(), experimental::Pauli::x);
        }
    }

    if (auto target_op = op.get_target_operation().get_underlying_if<qcir::PYGate>()) {
        if (op.get_num_qubits() == 2 && target_op->get_phase() == dvlab::Phase(1)) {
            tableau.sdg(qubits[1]);
            tableau.cx(qubits[0], qubits[1]);
            tableau.s(qubits[1]);
            return true;
        } else {
            return experimental::implement_mcp(tableau, qubits, target_op->get_phase(), experimental::Pauli::y);
        }
    }

    if (auto target_op = op.get_target_operation().get_underlying_if<qcir::PZGate>()) {
        if (op.get_num_qubits() == 2 && target_op->get_phase() == dvlab::Phase(1)) {
            tableau.cz(qubits[0], qubits[1]);
            return true;
        } else {
            return experimental::implement_mcp(tableau, qubits, target_op->get_phase(), experimental::Pauli::z);
        }
    }

    if (auto target_op = op.get_target_operation().get_underlying_if<qcir::RXGate>()) {
        return experimental::implement_mcr(tableau, qubits, target_op->get_phase(), experimental::Pauli::x);
    }

    if (auto target_op = op.get_target_operation().get_underlying_if<qcir::RYGate>()) {
        return experimental::implement_mcr(tableau, qubits, target_op->get_phase(), experimental::Pauli::y);
    }

    if (auto target_op = op.get_target_operation().get_underlying_if<qcir::RZGate>()) {
        return experimental::implement_mcr(tableau, qubits, target_op->get_phase(), experimental::Pauli::z);
    }

    return false;
}

namespace experimental {

std::optional<Tableau> to_tableau(qcir::QCir const& qcir) {
    Tableau result{qcir.get_num_qubits()};

    for (auto const& gate : qcir.get_gates()) {
        if (stop_requested()) {
            return std::nullopt;
        }
        if (!append_to_tableau(gate->get_operation(), result, gate->get_qubits())) {
            spdlog::error("Gate type {} is not supported!!", gate->get_operation().get_type());
            return std::nullopt;
        }
    }

    return result;
}

}  // namespace experimental

template <>
bool append_to_tableau(qcir::QCir const& qcir, experimental::Tableau& tableau, QubitIdList const& qubits) {
    auto qubit_map = std::unordered_map<QubitIdType, QubitIdType>();

    for (size_t i = 0; i < qcir.get_num_qubits(); ++i) {
        qubit_map[i] = qubits[i];
    }

    for (auto const& gate : qcir.get_gates()) {
        auto const gate_qubits = gate->get_qubits();
        if (!append_to_tableau(gate->get_operation(), tableau, gate_qubits | std::views::transform([&qubit_map](auto q) { return qubit_map[q]; }) | tl::to<QubitIdList>())) {
            spdlog::error("Gate type {} is not supported!!", gate->get_operation().get_type());
            return false;
        }
    }

    return true;
}

}  // namespace qsyn
