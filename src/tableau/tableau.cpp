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

#include <cstddef>

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
                    cct.operations().cx(control, target);
                }),
            subtableau);
        if (std::holds_alternative<StabilizerTableau>(subtableau))
            break;
    }
    return *this;
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



void Tableau::commute_classical(){
    if (_subtableaux.empty()) {
        return;
    }
    // Track the position where CCTs should be moved to (initially the end)
    std::vector<SubTableau> ccts;
    // Iterate through subtableaux in reverse order
    for (size_t idx = _subtableaux.size(); idx > 0; --idx) {
        size_t actual_idx = idx - 1;  // Convert to 0-based index        
        if (auto* cct = std::get_if<ClassicalControlTableau>(&_subtableaux[actual_idx])){
            for (size_t j = actual_idx + 1; j < _subtableaux.size(); ++j) {
                
                std::visit(
                    dvlab::overloaded{
                        [cct](StabilizerTableau& st) {
                            commute_through_stabilizer(*cct, st);  
                        },
                        [cct](std::vector<PauliRotation>& pr) {
                            commute_through_pauli_rotation(*cct, pr);
                        },
                        [](ClassicalControlTableau& /* other_cct */) {
                            // We should not encounter a ClassicalControlTableau here
                            spdlog::error("Unexpected ClassicalControlTableau encountered during commutation");
                        }},
                    _subtableaux[j]);                
            }
            ccts.insert(ccts.begin(), *cct);
            _subtableaux.erase(_subtableaux.begin() + actual_idx);
        }
    }
    spdlog::info("Commutation complete. Moving {} CCT(s) to end.", ccts.size());
    _subtableaux.insert(_subtableaux.end(), ccts.begin(), ccts.end());


}


}  // namespace qsyn::experimental
