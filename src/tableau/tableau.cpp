/**
 * @file tableau.cpp
 * @brief define tableau member functions
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "./tableau.hpp"
#include "./classical_tableau.hpp"
#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include <cassert>
#include <cstddef>
#include <functional>

namespace qsyn::experimental {

Tableau& Tableau::h(size_t qubit) noexcept {
    for (auto& subtableau : _subtableaux | std::views::reverse) {
        std::visit(
            dvlab::overloaded(
                [qubit](StabilizerTableau& subtableau) { subtableau.h(qubit); },
                [qubit](std::vector<PauliRotation>& subtableau) {
                    std::ranges::for_each(subtableau, [qubit](auto& rotation) { rotation.h(qubit); });
                },
                [qubit](ClassicalControlTableau& cct) {
                    assert((!cct.is_classical_control() || qubit != cct.ancilla_qubit()) &&
                           "Tableau::h: classical-control block must not act on ancilla qubit");
                    cct.operations().h(qubit);
                }),
            subtableau);
        if (std::holds_alternative<StabilizerTableau>(subtableau))
            break;
    }
    return *this;
}

Tableau& Tableau::s(size_t qubit) noexcept {
    for (auto& subtableau : _subtableaux | std::views::reverse) {
        std::visit(
            dvlab::overloaded(
                [qubit](StabilizerTableau& subtableau) { subtableau.s(qubit); },
                [qubit](std::vector<PauliRotation>& subtableau) {
                    std::ranges::for_each(subtableau, [qubit](auto& rotation) { rotation.s(qubit); });
                },
                [qubit](ClassicalControlTableau& cct) {
                    assert((!cct.is_classical_control() || qubit != cct.ancilla_qubit()) &&
                           "Tableau::s: classical-control block must not act on ancilla qubit");
                    cct.operations().s(qubit);
                }),
            subtableau);
        if (std::holds_alternative<StabilizerTableau>(subtableau))
            break;
    }
    return *this;
}

Tableau& Tableau::cx(size_t control, size_t target) noexcept {
    for (auto& subtableau : _subtableaux | std::views::reverse) {
        std::visit(
            dvlab::overloaded(
                [control, target](StabilizerTableau& subtableau) { subtableau.cx(control, target); },
                [control, target](std::vector<PauliRotation>& subtableau) {
                    std::ranges::for_each(subtableau, [control, target](auto& rotation) { rotation.cx(control, target); });
                },
                [control, target](ClassicalControlTableau& cct) {
                    assert((!cct.is_classical_control() ||
                            (control != cct.ancilla_qubit() && target != cct.ancilla_qubit())) &&
                           "Tableau::cx: classical-control block must not act on ancilla qubit");
                    cct.operations().cx(control, target);
                }),
            subtableau);
        if (std::holds_alternative<StabilizerTableau>(subtableau))
            break;
    }
    return *this;
}

namespace {

std::optional<GadgetPairIndices> find_gadget_pair_impl(
    size_t size,
    std::function<SubTableau const&(size_t)> const& at,
    size_t ancilla_qubit) {
    std::optional<size_t> gadget_index;
    for (size_t idx = 0; idx < size; ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&at(idx));
        if (cct == nullptr || !cct->is_gadget() || cct->ancilla_qubit() != ancilla_qubit) {
            continue;
        }
        gadget_index = idx;
        break;
    }
    if (!gadget_index.has_value()) {
        return std::nullopt;
    }

    for (size_t idx = gadget_index.value() + 1; idx < size; ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&at(idx));
        if (cct == nullptr || !cct->is_classical_control() || cct->ancilla_qubit() != ancilla_qubit) {
            continue;
        }
        return GadgetPairIndices{gadget_index.value(), idx};
    }
    return std::nullopt;
}

}  // namespace

std::optional<GadgetPairIndices> find_gadget_pair(
    std::vector<SubTableau> const& subtableaux,
    size_t ancilla_qubit) {
    return find_gadget_pair_impl(
        subtableaux.size(),
        [&](size_t idx) -> SubTableau const& { return subtableaux[idx]; },
        ancilla_qubit);
}

std::optional<GadgetPairIndices> find_gadget_pair(
    Tableau const& tableau,
    size_t ancilla_qubit) {
    return find_gadget_pair_impl(
        tableau.size(),
        [&](size_t idx) -> SubTableau const& { return tableau[idx]; },
        ancilla_qubit);
}

void adjoint_inplace(SubTableau& subtableau) {
    std::visit(
        dvlab::overloaded(
            [](StabilizerTableau& subtableau) { adjoint_inplace(subtableau); },
            [](std::vector<PauliRotation>& subtableau) {
                std::ranges::for_each(subtableau, [](PauliRotation& rotation) {
                    rotation.phase() *= -1;
                });
                std::ranges::reverse(subtableau);
            },
            [](ClassicalControlTableau& cct) {
                adjoint_inplace(cct.operations());
            }),
        subtableau);
}

SubTableau adjoint(SubTableau const& subtableau) {
    auto adjoint_subtableau = subtableau;
    adjoint_inplace(adjoint_subtableau);
    return adjoint_subtableau;
}

void adjoint_inplace(Tableau& tableau) {
    std::ranges::reverse(tableau);
    std::ranges::for_each(tableau, [](SubTableau& subtableau) { adjoint_inplace(subtableau); });
}

Tableau adjoint(Tableau const& tableau) {
    auto adjoint_tableau = tableau;
    adjoint_inplace(adjoint_tableau);
    return adjoint_tableau;
}


}  // namespace qsyn::experimental
