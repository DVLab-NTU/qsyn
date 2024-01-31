/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include "./tableau_optimization.hpp"

#include <functional>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>

#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"

namespace qsyn {

namespace experimental {

namespace {
/**
 * @brief A view of the conjugation of a Clifford and a list of PauliRotations.
 *
 */
class ConjugationView : public PauliProductTrait<ConjugationView> {
public:
    ConjugationView(
        StabilizerTableau& clifford,
        std::vector<PauliRotation>& rotations,
        size_t upto) : clifford{clifford}, rotations{rotations}, upto{upto} {}

    ConjugationView& h(size_t qubit) override {
        clifford.get().h(qubit);
        for (size_t i = 0; i < upto; ++i) {
            rotations.get()[i].h(qubit);
        }
        return *this;
    }

    ConjugationView& s(size_t qubit) override {
        clifford.get().s(qubit);
        for (size_t i = 0; i < upto; ++i) {
            rotations.get()[i].s(qubit);
        }
        return *this;
    }

    ConjugationView& cx(size_t control, size_t target) override {
        clifford.get().cx(control, target);
        for (size_t i = 0; i < upto; ++i) {
            rotations.get()[i].cx(control, target);
        }
        return *this;
    }

private:
    std::reference_wrapper<StabilizerTableau> clifford;
    std::reference_wrapper<std::vector<PauliRotation>> rotations;
    size_t upto;
};

/**
 * @brief remove the Pauli rotations with zero phase
 *
 * @param rotations
 */
void remove_rotations_with_zero_phase(std::vector<PauliRotation>& rotations) {
    rotations.erase(
        std::remove_if(
            rotations.begin(),
            rotations.end(),
            [](PauliRotation const& rotation) { return rotation.phase() == dvlab::Phase(0); }),
        rotations.end());
}

};  // namespace

/**
 * @brief merge rotations that are commutative and have the same underlying pauli product.
 *
 * @param rotations
 */
void merge_rotations(std::vector<PauliRotation>& rotations) {
    assert(std::ranges::all_of(rotations, [&rotations](PauliRotation const& rotation) { return rotation.n_qubits() == rotations.front().n_qubits(); }));

    // merge two rotations if they are commutative and have the same underlying pauli product
    for (size_t i = 0; i < rotations.size(); ++i) {
        for (size_t j = i + 1; j < rotations.size(); ++j) {
            if (!is_commutative(rotations[i], rotations[j])) break;
            if (rotations[i].pauli_product() == rotations[j].pauli_product()) {
                rotations[i].phase() += rotations[j].phase();
                rotations[j].phase() = dvlab::Phase(0);
            }
        }
    }

    // remove all rotations with zero phase
    remove_rotations_with_zero_phase(rotations);
}

/**
 * @brief merge rotations that are commutative and have the same underlying pauli product.
 *        If a rotation becomes Clifford, absorb it into the initial Clifford operator.
 *        This algorithm is inspired by the paper [[1903.12456] Optimizing T gates in Clifford+T circuit as $π/4$ rotations around Paulis](https://arxiv.org/abs/1903.12456)
 *
 * @param clifford
 * @param rotations
 */
void merge_rotations(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations) {
    merge_rotations(rotations);

    for (size_t i = 0; i < rotations.size(); ++i) {
        if (rotations[i].phase() != dvlab::Phase(1, 2) &&
            rotations[i].phase() != dvlab::Phase(-1, 2) &&
            rotations[i].phase() != dvlab::Phase(1)) continue;

        ConjugationView conjugation_view{clifford, rotations, i};

        for (size_t qb = 0; qb < rotations[i].n_qubits(); ++qb) {
            if (rotations[i].get_pauli_type(qb) == Pauli::X) {
                conjugation_view.h(qb);
            } else if (rotations[i].get_pauli_type(qb) == Pauli::Y) {
                conjugation_view.v(qb);
            }
        }

        // get all the qubits that are not I
        auto const non_I_qubits = std::views::iota(0ul, rotations[i].n_qubits()) |
                                  std::views::filter([&rotation = rotations[i]](auto qb) {
                                      return rotation.get_pauli_type(qb) != Pauli::I;
                                  }) |
                                  tl::to<std::vector>();

        for (auto const& [c, t] : tl::views::adjacent<2>(non_I_qubits)) {
            conjugation_view.cx(c, t);
        }

        if (rotations[i].phase() == dvlab::Phase(1, 2)) {
            conjugation_view.s(non_I_qubits.back());
        } else if (rotations[i].phase() == dvlab::Phase(-1, 2)) {
            conjugation_view.sdg(non_I_qubits.back());
        } else {
            assert(rotations[i].phase() == dvlab::Phase(1));
            conjugation_view.z(non_I_qubits.back());
        }
        rotations[i].phase() = dvlab::Phase(0);

        for (auto const& [t, c] : tl::views::adjacent<2>(non_I_qubits | std::views::reverse)) {
            conjugation_view.cx(c, t);
        }

        for (size_t qb = 0; qb < rotations[i].n_qubits(); ++qb) {
            if (rotations[i].get_pauli_type(qb) == Pauli::X) {
                conjugation_view.h(qb);
            } else if (rotations[i].get_pauli_type(qb) == Pauli::Y) {
                conjugation_view.vdg(qb);
            }
        }
    }

    // remove all rotations with zero phase
    remove_rotations_with_zero_phase(rotations);
}

}  // namespace experimental

}  // namespace qsyn
