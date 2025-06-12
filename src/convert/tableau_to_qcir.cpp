/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_to_qcir.hpp"

#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::tableau {

namespace detail {

using COT = CliffordOperatorType;

void add_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    auto const& [type, qubits] = op;

    switch (type) {
        case COT::h:
            qcir.append(qcir::HGate(), {qubits[0]});
            break;
        case COT::s:
            qcir.append(qcir::SGate(), {qubits[0]});
            break;
        case COT::cx:
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            break;
        case COT::sdg:
            qcir.append(qcir::SdgGate(), {qubits[0]});
            break;
        case COT::v:
            qcir.append(qcir::SXGate(), {qubits[0]});
            break;
        case COT::vdg:
            qcir.append(qcir::SXdgGate(), {qubits[0]});
            break;
        case COT::x:
            qcir.append(qcir::XGate(), {qubits[0]});
            break;
        case COT::y:
            qcir.append(qcir::YGate(), {qubits[0]});
            break;
        case COT::z:
            qcir.append(qcir::ZGate(), {qubits[0]});
            break;
        case COT::cz:
            qcir.append(qcir::CZGate(), {qubits[0], qubits[1]});
            break;
        case COT::swap:
            qcir.append(qcir::SwapGate(), {qubits[0], qubits[1]});
            break;
        case COT::ecr:
            qcir.append(qcir::ECRGate(), {qubits[0], qubits[1]});
            break;
    }
}

// TODO: merge with add_clifford_gate
void prepend_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    auto const& [type, qubits] = op;
    switch (type) {
        case COT::h:
            qcir.prepend(qcir::HGate(), {qubits[0]});
            break;
        case COT::s:
            qcir.prepend(qcir::SGate(), {qubits[0]});
            break;
        case COT::cx:
            qcir.prepend(qcir::CXGate(), {qubits[0], qubits[1]});
            break;
        case COT::sdg:
            qcir.prepend(qcir::SdgGate(), {qubits[0]});
            break;
        case COT::v:
            qcir.prepend(qcir::SXGate(), {qubits[0]});
            break;
        case COT::vdg:
            qcir.prepend(qcir::SXdgGate(), {qubits[0]});
            break;
        case COT::x:
            qcir.prepend(qcir::XGate(), {qubits[0]});
            break;
        case COT::y:
            qcir.prepend(qcir::YGate(), {qubits[0]});
            break;
        case COT::z:
            qcir.prepend(qcir::ZGate(), {qubits[0]});
            break;
        case COT::cz:
            qcir.prepend(qcir::CZGate(), {qubits[0], qubits[1]});
            break;
        case COT::swap:
            qcir.prepend(qcir::SwapGate(), {qubits[0], qubits[1]});
            break;
        case COT::ecr:
            qcir.prepend(qcir::ECRGate(), {qubits[0], qubits[1]});
            break;
    }   
}

void add_clifford_gate(PauliRotationTableau& rotations, CliffordOperator const& op) {
    auto const& [type, qubits] = op;

    switch (type) {
        case COT::h:
            for(auto& rot : rotations) {
                rot.h(qubits[0]);
            }
            break;
        case COT::s:
            for(auto& rot : rotations) {
                rot.s(qubits[0]);
            }
            break;
        case COT::cx:
            for(auto& rot : rotations) {
                rot.cx(qubits[0], qubits[1]);
            }
            break;
        case COT::v:
            for(auto& rot : rotations) {
                rot.h(qubits[0]);
                rot.s(qubits[0]);
                rot.h(qubits[0]);
            }
            break;
        default:
            spdlog::error("Invalid Clifford operator type {}. The operation is skipped.", to_string(type));
            break;
    }
}

void add_clifford_gate(StabilizerTableau& tableau, CliffordOperator const& op) {
    auto const& [type, qubits] = op;
    switch (type) {
        case COT::h:
            tableau.h(qubits[0]);
            break;
        case COT::s:
            tableau.s(qubits[0]);
            break;
        case COT::cx:
            tableau.cx(qubits[0], qubits[1]);
            break;
        case COT::sdg:
            tableau.sdg(qubits[0]);
            break;
        case COT::v:
            tableau.v(qubits[0]);
            break;
        case COT::vdg:
            tableau.vdg(qubits[0]);
    }
}

void prepend_clifford_gate(StabilizerTableau& tableau, CliffordOperator const& op) {
    tableau.prepend(op);
}

}  // namespace detail

/**
 * @brief convert a stabilizer tableau to a QCir.
 *
 * @param clifford - pass by value on purpose
 * @return std::optional<qcir::QCir>
 */
std::optional<qcir::QCir> to_qcir(
    StabilizerTableau const& clifford,
    StabilizerTableauSynthesisStrategy const& strategy) {
    qcir::QCir qcir{clifford.n_qubits()};
    for (auto const& op : extract_clifford_operators(clifford, strategy)) {
        if (stop_requested()) {
            return std::nullopt;
        }
        detail::add_clifford_gate(qcir, op);
    }

    return qcir;
}

/**
 * @brief convert a Pauli rotation to a QCir. This is a naive implementation.
 *
 * @param pauli_rotation
 * @return qcir::QCir
 */
std::optional<qcir::QCir> to_qcir(
    std::vector<PauliRotation> const& pauli_rotations,
    PauliRotationsSynthesisStrategy const& strategy) {
    return strategy.synthesize(pauli_rotations);
}

namespace {

std::optional<qcir::QCir> to_qcir_eager(
    Tableau const& tableau,
    StabilizerTableauSynthesisStrategy const& st_strategy,
    PauliRotationsSynthesisStrategy const& pr_strategy) {
    qcir::QCir qcir{tableau.n_qubits()};

    size_t iter = 0;
    for (auto const& subtableau : tableau) {
        if (stop_requested()) {
            return std::nullopt;
        }
        auto const qc_fragment =
            std::visit(
                dvlab::overloaded{
                    [&st_strategy](StabilizerTableau const& st) {
                        return to_qcir(st, st_strategy);
                    },
                    [&pr_strategy](std::vector<PauliRotation> const& pr) {
                        return to_qcir(pr, pr_strategy);
                    }},
                subtableau);
        if (!qc_fragment) {
            return std::nullopt;
        }
        auto const gate_stats = get_gate_statistics(*qc_fragment);
        auto const cx_gate_count =
            gate_stats.contains("cx") ? gate_stats.at("cx") : 0;
        spdlog::info("CX gate count in the subtableau {}: {}", iter, cx_gate_count);
        qcir.compose(*qc_fragment);
        iter++;
    }

    return qcir;
}

/**
 * @brief check if the subtableaux alternate between
 * stabilizer and Pauli rotations
 *
 * @param tableau
 * @return true
 * @return false
 */
bool is_alternating(Tableau const& tableau) {
    // NOLINTBEGIN(readability-use-anyofallof)
    //    : tl::views::pairwise is not supported by anyof/allof
    for (auto const& [sub1, sub2] : tl::views::pairwise(tableau)) {
        if (std::holds_alternative<StabilizerTableau>(sub1) ==
            std::holds_alternative<StabilizerTableau>(sub2)) {
            return false;
        }
    }
    // NOLINTEND(readability-use-anyofallof)
    return true;
}

void synthesize_clifford_until_h_free(
    qcir::QCir& qcir,
    StabilizerTableau const& this_clifford,
    PauliRotationTableau& prt,
    StabilizerTableau& next_clifford, 
    size_t iter) {
    // synthesize until the H-layer
    // the H-opt synthesis strategy put the H-optimal circuit
    // at the end. however, we want to synthesize them first.
    // As a result, we need to adjoint the stabilizer tableau first.
    auto const st_strategy = HOptSynthesisStrategy{};
    auto clifford_adj      = adjoint(this_clifford);

    auto const diag_gates =
        st_strategy.partial_synthesize(clifford_adj);

    size_t cx_gate_count = 0;
    for (auto const& op : diag_gates) {
        detail::add_clifford_gate(qcir, op);
        if (op.first == CliffordOperatorType::cx) {
            cx_gate_count++;
        }
    }
    spdlog::info("CX gate count in the Clifford segment {}: {}", iter, cx_gate_count);

    // now, [diag_gates] -- [rem_gates] -- [prt] -- [next_clifford]
    // implements the desired transformation.
    // To absorb rem_gates into the next Clifford, we
    //   1. conjugate the prt with the (rem_gates)^T, and
    //   2. prepend the next_clifford with (rem_gates)
    auto const rem_gates_adj =
        // Since the rem_gates will be absorbed into subsequent operators,
        // we use the simplest AG synthesis strategy to extract them.
        extract_clifford_operators(clifford_adj, AGSynthesisStrategy{});
    auto const rem_gates = adjoint(rem_gates_adj);
    assert(std::ranges::all_of(rem_gates, [](auto const& op) {
        return op.first != CliffordOperatorType::h;
    }));

    for (auto& rot : prt) {
        rot.apply(rem_gates_adj);
    }
    next_clifford.prepend(rem_gates);
}

std::optional<qcir::QCir>
to_qcir_lazy(
    Tableau tableau,  // takes copy to avoid modifying the original tableau
    PartialPauliRotationsSynthesisStrategy const& pr_strategy) {
    fmt::println("Note: lazy synthesis is not stable. Use at your own risk!!");
    if (!is_alternating(tableau)) {
        spdlog::error(
            "Subtableaux must alternate between "
            "stabilizer and Pauli rotations!!");
        return std::nullopt;
    }

    // ensure the last subtableau is a stabilizer tableau
    if (std::holds_alternative<PauliRotationTableau>(tableau.back())) {
        tableau.push_back(StabilizerTableau{tableau.n_qubits()});
    }

    auto const st_strategy = HOptSynthesisStrategy{};

    qcir::QCir qcir{tableau.n_qubits()};

    size_t clifford_segment_idx = 0;

    for (size_t i = 0; i < tableau.size() - 1; ++i) {
        bool const current_is_st =
            std::holds_alternative<StabilizerTableau>(tableau[i]);
        if (current_is_st) {
            // the next Clifford always exists because we pushed a final
            // identity stabilizer tableau to the end of the tableau.
            synthesize_clifford_until_h_free(
                qcir,
                std::get<StabilizerTableau>(tableau[i]),
                std::get<PauliRotationTableau>(tableau[i + 1]),
                std::get<StabilizerTableau>(tableau[i + 2]),
                clifford_segment_idx);
            clifford_segment_idx++;
        } else {
            auto const& prt =
                std::get<PauliRotationTableau>(tableau[i]);
            auto& next_clifford =
                std::get<StabilizerTableau>(tableau[i + 1]);

            auto result = pr_strategy.partial_synthesize(
                prt);
            if (!result) {
                return std::nullopt;
            }

            auto [qc_fragment, rem_clifford] = std::move(*result);

            // delay the CX tableau synthesis by pushing it into the
            // next stabilizer tableau
            qcir.compose(qc_fragment);
            next_clifford.prepend(rem_clifford);
        }
    }

    // synthesize the last subtableau
    auto const last_qc_fragment =
        to_qcir(std::get<StabilizerTableau>(tableau.back()), st_strategy);
    if (!last_qc_fragment) {
        return std::nullopt;
    }

    auto const gate_stats = get_gate_statistics(*last_qc_fragment);
    auto const cx_gate_count =
        gate_stats.contains("cx") ? gate_stats.at("cx") : 0;
    fmt::println("CX gate count in the last Clifford: {}", cx_gate_count);

    qcir.compose(*last_qc_fragment);

    return qcir;
}

std::optional<qcir::QCir>
to_qcir_backward(
    Tableau tableau,
    BackwardPartialPauliRotationsSynthesisStrategy const& pr_strategy,
    StabilizerTableauSynthesisStrategy const& st_strategy) {

    if (tableau.size() != 2 || !std::holds_alternative<StabilizerTableau>(tableau[0]) || !std::holds_alternative<PauliRotationTableau>(tableau[1])) {
        spdlog::error("Tableau should be propogated to the end!!");
        return std::nullopt;
    }

    auto& initial_clifford = std::get<StabilizerTableau>(tableau[0]);
    auto const& rotations = std::get<PauliRotationTableau>(tableau[1]);

    auto result = pr_strategy.backward_synthesize(rotations, initial_clifford);
    if (!result) {
        return std::nullopt;
    }
    auto initial_clifford_qcir = to_qcir(initial_clifford, st_strategy);
    if (!initial_clifford_qcir) {
        return std::nullopt;
    }
    auto const gate_stats = get_gate_statistics(*initial_clifford_qcir);
    auto const cx_gate_count =
        gate_stats.contains("cx") ? gate_stats.at("cx") : 0;
    spdlog::info("CX gate count in the initial Clifford: {}", cx_gate_count);
    initial_clifford_qcir->compose(*result);
    
    return initial_clifford_qcir;
}


}  // namespace

/**
 * @brief convert a tableau to a QCir.
 *
 * @param tableau
 * @param st_strategy
 * @param pr_strategy
 * @param lazy
 */
std::optional<qcir::QCir> to_qcir(
    Tableau const& tableau,
    StabilizerTableauSynthesisStrategy const& st_strategy,
    PauliRotationsSynthesisStrategy const& pr_strategy,
    bool lazy, bool backward) {
    if (lazy) {
        auto const* pr_strategy_ptr =
            dynamic_cast<PartialPauliRotationsSynthesisStrategy const*>(
                &pr_strategy);
        if (pr_strategy_ptr) {
            return to_qcir_lazy(
                tableau, *pr_strategy_ptr);
        }
        spdlog::error("Lazy synthesis requires a partially-synthesizable Pauli rotations synthesis strategy!!");
        return std::nullopt;
    }
    if (backward) {
        auto const* pr_strategy_ptr =
            dynamic_cast<BackwardPartialPauliRotationsSynthesisStrategy const*>(
                &pr_strategy);
        if (pr_strategy_ptr) {
            return to_qcir_backward(tableau, *pr_strategy_ptr, st_strategy);
        }
        spdlog::error("Backward synthesis requires a backward-synthesizable Pauli rotations synthesis strategy!!");
        return std::nullopt;
    }
    return to_qcir_eager(tableau, st_strategy, pr_strategy);
}

}  // namespace qsyn::tableau
