
#include <gsl/narrow>
#include <random>
#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include "convert/tableau_to_qcir.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

namespace qsyn::tableau {

namespace {
/**
 * @brief select a row consisting completely of 1s to be the target row.
 *
 * @param rotations
 * @param rotation_filter
 * @param pivot
 * @return size_t
 */
std::vector<size_t>
get_control_rows(
    std::vector<PauliRotation> const& rotations,
    std::vector<size_t> const& rotation_filter,
    size_t pivot) {
    auto const num_qubits = rotations.front().n_qubits();
    auto control_rows     = std::vector<size_t>{};
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (i == pivot) continue;

        if (std::ranges::all_of(
                rotation_filter,
                [&](auto x) {
                    return rotations[x].pauli_product().is_z_set(i);
                })) {
            control_rows.push_back(i);
        }
    }

    return control_rows;
}

void apply_cxs(
    std::vector<size_t> ctrls,
    size_t targ,
    GraySynthStrategy::Mode mode,
    std::vector<PauliRotation>& rotations,
    qcir::QCir& qcir,
    StabilizerTableau& final_clifford,
    std::unordered_set<std::size_t> const& frozen_rotations,
    std::size_t num_rotations,
    std::vector<std::size_t> const& random_order) {
    using Mode = GraySynthStrategy::Mode;

    auto const apply_cx = [&](size_t ctrl, size_t targ) {
        for (auto col_id : std::views::iota(0ul, num_rotations)) {
            if (!frozen_rotations.contains(col_id)) {
                rotations[col_id].cx(ctrl, targ);
            }
        }
        qcir.append(qcir::CXGate(), {ctrl, targ});
        final_clifford.prepend_cx(ctrl, targ);
    };

    switch (mode) {
        case Mode::star:
            for (auto ctrl : ctrls) {
                apply_cx(ctrl, targ);
            }
            break;
        case Mode::staircase:
            // sort the controls according to the random_order
            std::ranges::sort(ctrls, [&](auto const& x, auto const& y) {
                return random_order[x] < random_order[y];
            });
            for (auto&& [c, t] : ctrls | tl::views::pairwise) {
                apply_cx(c, t);
            }
            if (!ctrls.empty()) {
                apply_cx(ctrls.back(), targ);
            }
            break;
    }
}

/**
 * @brief select a row with the most or least number of 1.
 *
 * @param rotations
 * @param rotation_filter
 * @param qubit_filter
 * @return size_t
 */
size_t
get_cofactor_row(
    std::vector<PauliRotation> const& rotations,
    std::vector<size_t> const& rotation_filter,
    std::vector<size_t> const& qubit_filter) {
    auto counts = std::vector<std::size_t>(qubit_filter.size(), 0);
    for (auto col_id : rotation_filter) {
        for (auto&& [idx, qubit] : tl::views::enumerate(qubit_filter)) {
            if (rotations[col_id].pauli_product().is_z_set(qubit)) {
                counts[idx]++;
            }
        }
    }

    auto const [min_it, max_it] = std::ranges::minmax_element(counts);
    auto const most_ones        = std::distance(counts.begin(), max_it);
    auto const most_zeros       = std::distance(counts.begin(), min_it);

    if (counts[most_ones] >= rotation_filter.size() - counts[most_zeros]) {
        return qubit_filter[most_ones];
    } else {
        return qubit_filter[most_zeros];
    }
}

/**
 * @brief filter out a number from a vector.
 *
 * @param vec
 * @param num
 * @return std::vector<std::size_t>
 */
std::vector<std::size_t>
filter_out_number(
    std::vector<std::size_t> const& vec,
    std::size_t num) {
    return vec |
           std::views::filter([&](auto const& x) { return x != num; }) |
           tl::to<std::vector>();
}

}  // namespace

std::optional<PartialSynthesisResult>
GraySynthStrategy::partial_synthesize(
    std::vector<PauliRotation> const& rotations) const {
    auto const num_qubits    = rotations.front().n_qubits();
    auto const num_rotations = rotations.size();

    if (num_qubits == 0) {
        return PartialSynthesisResult{
            qcir::QCir{0},
            StabilizerTableau{num_qubits}};
    }

    if (num_rotations == 0) {
        return PartialSynthesisResult{
            qcir::QCir{num_qubits},
            StabilizerTableau{num_qubits}};
    }

    // checks if all rotations are diagonal
    if (!std::ranges::all_of(rotations, &PauliRotation::is_diagonal)) {
        spdlog::error("GraySynth only supports diagonal rotations");
        return std::nullopt;
    }

    auto frozen_rotations =
        std::unordered_set<std::size_t>{};  // ids to the rotations that
                                            // have been synthesized

    auto copy_rotations = rotations;

    using stack_elem_t =
        std::tuple<
            std::vector<std::size_t>,  // rotation filter
            std::vector<std::size_t>,  // qubit filter
            size_t>;                   // target row
    auto stack = std::vector<stack_elem_t>{};

    stack.emplace_back(
        std::views::iota(0ul, num_rotations) | tl::to<std::vector>(),
        std::views::iota(0ul, num_qubits) | tl::to<std::vector>(),
        SIZE_MAX);

    auto qcir = qcir::QCir{copy_rotations.front().n_qubits()};

    StabilizerTableau final_clifford{num_qubits};

    // generate 0..num_qubits random order
    static auto rng = std::mt19937{42};
    auto random_order =
        std::views::iota(0ul, num_qubits) | tl::to<std::vector>();
    std::ranges::shuffle(random_order, rng);

    while (!stack.empty()) {
        auto const [rotation_filter, qubit_filter, targ] =
            std::move(stack.back());
        stack.pop_back();
        if (rotation_filter.empty()) continue;
        if (targ != SIZE_MAX) {
            auto ctrls =
                get_control_rows(copy_rotations, rotation_filter, targ);

            apply_cxs(
                std::move(ctrls), targ, mode,
                copy_rotations,
                qcir, final_clifford,
                frozen_rotations, num_rotations, random_order);
        }

        if (qubit_filter.empty()) {
            for (auto col_id : rotation_filter) {
                if (frozen_rotations.contains(col_id)) continue;
                frozen_rotations.insert(col_id);
                DVLAB_ASSERT(
                    targ < num_qubits,
                    "`targ` should be a valid qubit index");
                qcir.append(
                    qcir::PZGate(copy_rotations[col_id].phase()),
                    {targ});
            }
            continue;
        }

        auto const row_id = get_cofactor_row(
            copy_rotations,
            rotation_filter,
            qubit_filter);

        auto const zero_rotations =
            rotation_filter |
            std::views::filter([&](auto const& x) {
                return !copy_rotations[x].pauli_product().is_z_set(row_id);
            }) |
            tl::to<std::vector>();
        auto const one_rotations =
            rotation_filter |
            std::views::filter([&](auto const& x) {
                return copy_rotations[x].pauli_product().is_z_set(row_id);
            }) |
            tl::to<std::vector>();

        stack.emplace_back(
            zero_rotations,
            filter_out_number(qubit_filter, row_id),
            targ);
        stack.emplace_back(
            one_rotations,
            filter_out_number(qubit_filter, row_id),
            targ == SIZE_MAX ? row_id : targ);
    }

    return PartialSynthesisResult{
        std::move(qcir),
        std::move(final_clifford)};
}

std::optional<qcir::QCir>
GraySynthStrategy::synthesize(
    std::vector<PauliRotation> const& rotations) const {
    auto partial_result = partial_synthesize(rotations);
    if (!partial_result) {
        return std::nullopt;
    }

    auto [qcir, final_clifford] = std::move(*partial_result);

    // it seems like gaussian is the best
    auto const final_cxs = synthesize_cx_gaussian(final_clifford);

    for (auto const& cx : final_cxs) {
        detail::add_clifford_gate(qcir, cx);
    }

    return qcir;
}

}  // namespace qsyn::tableau
