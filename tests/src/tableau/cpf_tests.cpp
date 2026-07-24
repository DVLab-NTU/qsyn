#include <catch2/catch_test_macros.hpp>

#include "tableau/cpf/angle_utils.hpp"
#include "tableau/cpf/propagation_merge.hpp"
#include "tableau/cpf/trace_replay.hpp"
#include "tableau/pauli_dag/dag_fold.hpp"
#include "tableau/pauli_dag/pauli_dag.hpp"
#include "tableau/pauli_dag/staq_fold.hpp"
#include "tableau/pauli_dag/timeline.hpp"
#include "tableau/pauli_rotation.hpp"

using namespace qsyn::experimental;
using namespace qsyn::experimental::cpf;
using namespace qsyn::experimental::cpf::pauli_dag;

TEST_CASE("cpf angle_utils", "[cpf][angle_utils]") {
    using qsyn::Phase;
    REQUIRE(is_zero_phase(Phase(0)));
    REQUIRE(is_clifford_phase(Phase(1, 2)));
    REQUIRE(is_pi_phase(Phase(1)));
    REQUIRE_FALSE(is_clifford_phase(Phase(1, 3)));
}

TEST_CASE("cpf propagation_merge same_pauli", "[cpf][propagation_merge]") {
    using qsyn::Phase;
    PauliRotation left({Pauli::z}, Phase(1, 5));
    PauliRotation right({Pauli::z}, Phase(2, 5));
    StabilizerTableau  id{1};
    auto const         rep = classify_segment(left, id, right);
    REQUIRE(rep.klass == MergeClass::same_pauli);
    REQUIRE(rep.sign == 1);
}

TEST_CASE("pauli_dag global prune", "[cpf][pauli_dag]") {
    using qsyn::Phase;
    Tableau tab{2};
    tab.emplace_back(std::vector<PauliRotation>{
        PauliRotation({Pauli::z, Pauli::i}, Phase(1, 8)),
        PauliRotation({Pauli::z, Pauli::i}, Phase(-1, 8)),
    });
    auto const before = tab.n_pauli_rotations();
    auto const stats  = global_prune_pass(tab);
    REQUIRE(stats.n_rotations_pruned >= 1);
    REQUIRE(tab.n_pauli_rotations() < before);
}

TEST_CASE("staq_fold cx rz pair", "[cpf][staq_fold]") {
    using qsyn::Phase;
    Timeline tl{2};
    tl.entries.emplace_back(PauliRotation({Pauli::z, Pauli::i}, Phase(1, 3)));
    StabilizerTableau cx{2};
    cx.cx(0, 1);
    tl.entries.emplace_back(cx);
    tl.entries.emplace_back(PauliRotation({Pauli::z, Pauli::i}, Phase(2, 5)));
    StabilizerTableau cx2{2};
    cx2.cx(0, 1);
    tl.entries.emplace_back(cx2);

    auto const stats = staq_fold_timeline(tl);
    REQUIRE(stats.n_merges >= 1);
    REQUIRE(count_rotations(tl) == 1);
}

TEST_CASE("dag_fold h sandwich", "[cpf][dag_fold]") {
    using qsyn::Phase;
    Tableau tab{1};
    StabilizerTableau cliff{1};
    cliff.h(0);
    tab.emplace_back(cliff);
    tab.emplace_back(std::vector<PauliRotation>{PauliRotation({Pauli::x}, Phase(1, 5))});
    StabilizerTableau cliff2{1};
    cliff2.h(0);
    tab.emplace_back(cliff2);
    tab.emplace_back(std::vector<PauliRotation>{
        PauliRotation({Pauli::z}, Phase(1, 7)),
        PauliRotation({Pauli::x}, Phase(1, 9)),
    });

    auto const stats = dag_fold(tab);
    REQUIRE(stats.n_passes >= 1);
    REQUIRE(tab.n_pauli_rotations() <= 3);
}
