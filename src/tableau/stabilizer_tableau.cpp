/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define tableau base class and its derived classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./stabilizer_tableau.hpp"

#include <ranges>
#include <sul/dynamic_bitset.hpp>
#include <tl/adjacent.hpp>
#include <tl/to.hpp>
#include <unordered_set>

#include "tableau/pauli_rotation.hpp"

bool stop_requested();

namespace qsyn::tableau {

std::string StabilizerTableau::to_string() const {
    std::string ret;
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("S{}  {:+c}\n", i, _stabilizers[stabilizer_idx(i)]);
    }
    ret += '\n';
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("D{}  {:+c}\n", i, _stabilizers[destabilizer_idx(i)]);
    }
    return ret;
}

std::string StabilizerTableau::to_bit_string() const {
    std::string ret;
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("S{}  {:b}\n", i, _stabilizers[stabilizer_idx(i)]);
    }
    ret += '\n';
    for (size_t i = 0; i < n_qubits(); ++i) {
        ret += fmt::format("D{}  {:b}\n", i, _stabilizers[destabilizer_idx(i)]);
    }
    return ret;
}

StabilizerTableau& StabilizerTableau::h(size_t qubit) noexcept {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [qubit](PauliProduct& p) { p.h(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::s(size_t qubit) noexcept {
    if (qubit >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [qubit](PauliProduct& p) { p.s(qubit); });
    return *this;
}

StabilizerTableau& StabilizerTableau::cx(size_t ctrl, size_t targ) noexcept {
    if (ctrl >= n_qubits() || targ >= n_qubits()) return *this;
    std::ranges::for_each(_stabilizers, [ctrl, targ](PauliProduct& p) { p.cx(ctrl, targ); });
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_h(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    std::swap(stabilizer(qubit), destabilizer(qubit));
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_s(size_t qubit) {
    if (qubit >= n_qubits()) return *this;
    destabilizer(qubit) = stabilizer(qubit) * destabilizer(qubit);
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_cx(size_t ctrl, size_t targ) {
    if (ctrl >= n_qubits() || targ >= n_qubits()) return *this;
    stabilizer(targ)   = stabilizer(ctrl) * stabilizer(targ);
    destabilizer(ctrl) = destabilizer(targ) * destabilizer(ctrl);
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend_sdg(size_t qubit) {
    return prepend_s(qubit).prepend_s(qubit).prepend_s(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_v(size_t qubit) {
    return prepend_h(qubit).prepend_s(qubit).prepend_h(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_vdg(size_t qubit) {
    return prepend_h(qubit).prepend_sdg(qubit).prepend_h(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_x(size_t qubit) {
    return prepend_h(qubit).prepend_z(qubit).prepend_h(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_y(size_t qubit) {
    return prepend_x(qubit).prepend_z(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_z(size_t qubit) {
    return prepend_s(qubit).prepend_s(qubit);
}

StabilizerTableau& StabilizerTableau::prepend_cz(size_t ctrl, size_t targ) {
    return prepend_h(targ).prepend_cx(ctrl, targ).prepend_h(targ);
}

StabilizerTableau& StabilizerTableau::prepend_swap(size_t a, size_t b) {
    return prepend_cx(a, b).prepend_cx(b, a).prepend_cx(a, b);
}

StabilizerTableau& StabilizerTableau::prepend_ecr(size_t ctrl, size_t targ) {
    return prepend_x(ctrl).prepend_s(ctrl).prepend_v(targ).prepend_cx(ctrl, targ);
}

StabilizerTableau& StabilizerTableau::prepend(CliffordOperator const& op) {
    auto const& [type, qubits] = op;
    switch (type) {
        case CliffordOperatorType::h:
            return prepend_h(qubits[0]);
        case CliffordOperatorType::s:
            return prepend_s(qubits[0]);
        case CliffordOperatorType::cx:
            return prepend_cx(qubits[0], qubits[1]);
        case CliffordOperatorType::sdg:
            return prepend_sdg(qubits[0]);
        case CliffordOperatorType::v:
            return prepend_v(qubits[0]);
        case CliffordOperatorType::vdg:
            return prepend_vdg(qubits[0]);
        case CliffordOperatorType::x:
            return prepend_x(qubits[0]);
        case CliffordOperatorType::y:
            return prepend_y(qubits[0]);
        case CliffordOperatorType::z:
            return prepend_z(qubits[0]);
        case CliffordOperatorType::cz:
            return prepend_cz(qubits[0], qubits[1]);
        case CliffordOperatorType::swap:
            return prepend_swap(qubits[0], qubits[1]);
        case CliffordOperatorType::ecr:
            return prepend_ecr(qubits[0], qubits[1]);
    }
    DVLAB_UNREACHABLE("Every Clifford type should be handled in the switch-case");
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend(CliffordOperatorString const& ops) {
    for (auto const& op : ops | std::views::reverse) {
        prepend(op);
    }
    return *this;
}

StabilizerTableau& StabilizerTableau::prepend(StabilizerTableau const& tableau) {
    auto const ops = extract_clifford_operators(tableau);
    return prepend(ops);
}

StabilizerTableau adjoint(StabilizerTableau const& tableau) {
    return StabilizerTableau{tableau.n_qubits()}.apply(adjoint(extract_clifford_operators(tableau)));
}

CliffordOperatorString extract_clifford_operators(StabilizerTableau copy, StabilizerTableauSynthesisStrategy const& strategy) {
    return strategy.synthesize(std::move(copy));
}

namespace {
void add_cx(StabilizerTableau& tableau, size_t ctrl, size_t targ, CliffordOperatorString& clifford_ops) {
    tableau.cx(ctrl, targ);
    clifford_ops.push_back({CliffordOperatorType::cx, {ctrl, targ}});
}

void add_h(StabilizerTableau& tableau, size_t qubit, CliffordOperatorString& clifford_ops) {
    tableau.h(qubit);
    clifford_ops.push_back({CliffordOperatorType::h, {qubit, 0}});
}

void add_s(StabilizerTableau& tableau, size_t qubit, CliffordOperatorString& clifford_ops) {
    tableau.s(qubit);
    clifford_ops.push_back({CliffordOperatorType::s, {qubit, 0}});
}

void add_x(StabilizerTableau& tableau, size_t qubit, CliffordOperatorString& clifford_ops) {
    tableau.x(qubit);
    clifford_ops.push_back({CliffordOperatorType::x, {qubit, 0}});
}

void add_z(StabilizerTableau& tableau, size_t qubit, CliffordOperatorString& clifford_ops) {
    tableau.z(qubit);
    clifford_ops.push_back({CliffordOperatorType::z, {qubit, 0}});
}

void handle_negatives(StabilizerTableau& tableau, CliffordOperatorString& ops) {
    for (size_t qubit = 0; qubit < tableau.n_qubits(); ++qubit) {
        if (tableau.stabilizer(qubit).is_neg()) {
            add_x(tableau, qubit, ops);
        }
        if (tableau.destabilizer(qubit).is_neg()) {
            add_z(tableau, qubit, ops);
        }
    }
}

}  // namespace

CliffordOperatorString AGSynthesisStrategy::synthesize(StabilizerTableau copy) const {
    CliffordOperatorString clifford_ops;

    auto const make_destab_x_main_diag_1 = [&](size_t qubit) {
        if (copy.destabilizer(qubit).is_x_set(qubit)) return;

        auto const search_idx_range =
            std::views::iota(qubit + 1, copy.n_qubits());
        auto const ctrl = gsl::narrow<size_t>(
            std::ranges::find_if(
                search_idx_range, [&, qubit](size_t t) {
                    return copy.destabilizer(qubit).is_x_set(t);
                }) -
            search_idx_range.begin() + qubit + 1);

        if (ctrl < copy.n_qubits()) {
            add_cx(copy, ctrl, qubit, clifford_ops);
            return;
        }

        for (size_t ctrl = qubit; ctrl < copy.n_qubits(); ++ctrl) {
            if (copy.destabilizer(qubit).is_z_set(ctrl)) {
                add_h(copy, ctrl, clifford_ops);
                if (ctrl != qubit) {
                    add_cx(copy, ctrl, qubit, clifford_ops);
                }
                break;
            }
        }
    };

    auto const make_destab_x_off_diag_0 = [&](size_t qubit) {
        for (size_t targ = qubit + 1; targ < copy.n_qubits(); ++targ) {
            if (copy.destabilizer(qubit).is_x_set(targ)) {
                add_cx(copy, qubit, targ, clifford_ops);
            }
        }

        bool const some_z_set = std::ranges::any_of(
            std::views::iota(qubit, copy.n_qubits()),
            [&, qubit](size_t t) {
                return copy.destabilizer(qubit).is_z_set(t);
            });

        if (some_z_set) {
            if (!copy.destabilizer(qubit).is_z_set(qubit)) {
                add_s(copy, qubit, clifford_ops);
            }

            for (size_t ctrl = qubit + 1; ctrl < copy.n_qubits(); ++ctrl) {
                if (copy.destabilizer(qubit).is_z_set(ctrl)) {
                    add_cx(copy, ctrl, qubit, clifford_ops);
                }
            }
            add_s(copy, qubit, clifford_ops);
        }
    };

    auto const make_stab_z_off_diag_0 = [&](size_t qubit) {
        for (size_t ctrl = qubit + 1; ctrl < copy.n_qubits(); ++ctrl) {
            if (copy.stabilizer(qubit).is_z_set(ctrl)) {
                add_cx(copy, ctrl, qubit, clifford_ops);
            }
        }

        bool const some_x_set = std::ranges::any_of(
            std::views::iota(qubit, copy.n_qubits()),
            [&, qubit](size_t t) {
                return copy.stabilizer(qubit).is_x_set(t);
            });

        if (some_x_set) {
            add_h(copy, qubit, clifford_ops);

            for (size_t targ = qubit + 1; targ < copy.n_qubits(); ++targ) {
                if (copy.stabilizer(qubit).is_x_set(targ)) {
                    add_cx(copy, qubit, targ, clifford_ops);
                }
            }

            if (copy.stabilizer(qubit).is_z_set(qubit)) {
                add_s(copy, qubit, clifford_ops);
            }

            add_h(copy, qubit, clifford_ops);
        }
    };

    for (size_t qubit = 0; qubit < copy.n_qubits(); ++qubit) {
        if (stop_requested()) break;
        make_destab_x_main_diag_1(qubit);
        make_destab_x_off_diag_0(qubit);
        make_stab_z_off_diag_0(qubit);
    }

    if (stop_requested()) return {};

    handle_negatives(copy, clifford_ops);

    adjoint_inplace(clifford_ops);

    return clifford_ops;
}

namespace {
std::vector<size_t>
get_qubits_with_stabilizer_set(StabilizerTableau const& tableau, size_t qubit) {
    std::vector<size_t> qubits_with_stabilizer_set;
    for (size_t i = 0; i < tableau.n_qubits(); ++i) {
        if (tableau.stabilizer(qubit).is_x_set(i)) {
            qubits_with_stabilizer_set.push_back(i);
        }
    }
    return qubits_with_stabilizer_set;
}
}  // namespace

/**
 * @brief Synthesize the diagonal part of the tableau using the H-opt method.
 *        Note that the returned CliffordOperatorString is the adjoint of the
 *        actual diagonalization. This is because when we synthesize the
 *        remaining Clifford operators, we also synthesize an adjoint circuit.
 *        The caller is responsible for adjointing the gate string.
 *
 * @param clifford The tableau to synthesize.
 * @return The adjoint of the diagonalization.
 */
CliffordOperatorString
HOptSynthesisStrategy::partial_synthesize(StabilizerTableau& clifford) const {
    CliffordOperatorString diag_ops;

    // diagonalize all stabilizers
    for (size_t i = 0; i < clifford.n_qubits(); ++i) {
        if (stop_requested()) break;
        auto const qubit_range = std::views::iota(0ul, clifford.n_qubits());
        auto const qubits_with_stabilizer_set =
            get_qubits_with_stabilizer_set(clifford, i);

        if (qubits_with_stabilizer_set.empty()) continue;

        auto const ctrl = qubits_with_stabilizer_set.front();
        if (mode == Mode::star) {
            for (auto const targ :
                 qubits_with_stabilizer_set | std::views::drop(1)) {
                add_cx(clifford, ctrl, targ, diag_ops);
            }

        } else /* staircase */ {
            for (auto&& [t, c] :
                 qubits_with_stabilizer_set |
                     std::views::reverse |
                     tl::views::pairwise) {
                add_cx(clifford, c, t, diag_ops);
            }
        }

        if (clifford.stabilizer(i).is_z_set(ctrl)) {
            add_s(clifford, ctrl, diag_ops);
        }

        add_h(clifford, ctrl, diag_ops);
    }

    return diag_ops;
}

/**
 * @brief Synthesize from stabilizer tableau using the H-opt method.
 *        This function synthesizes the diagonal part of the tableau using the
 *        H-opt method, and then synthesizes the remaining Clifford operators
 *        using the Aaronson-Gottesman method. Note that the A-G subcircuit is
 *        placed before the H-opt subcircuit.
 *
 * @param copy The tableau to synthesize.
 * @return The synthesized CliffordOperatorString.
 */
CliffordOperatorString
HOptSynthesisStrategy::synthesize(StabilizerTableau copy) const {
    CliffordOperatorString const diag_ops = adjoint(partial_synthesize(copy));

    if (stop_requested()) return {};

    // synthesize the now diagonal stabilizers with Aaronson-Gottesman method

    // assert that stab_x are all zeros. This means that the tableau is H-free

    for (size_t i = 0; i < copy.n_qubits(); ++i) {
        for (size_t j = 0; j < copy.n_qubits(); ++j) {
            if (copy.stabilizer(i).is_x_set(j)) {
                DVLAB_ASSERT(false, "Stabilizer tableau is not H-free");
            }
        }
    }

    auto clifford_ops = extract_clifford_operators(copy, AGSynthesisStrategy{});
    // auto clifford_ops = synthesize_h_free_mr(copy);

    assert(std::ranges::none_of(clifford_ops, [](auto const& op) {
        return op.first == CliffordOperatorType::h;
    }));

    clifford_ops.insert(clifford_ops.end(), diag_ops.begin(), diag_ops.end());

    return clifford_ops;
}

namespace {

// helper function that converts a row operation to a CX gate
// This function is used to avoid confusion because the control-target direction
// is opposite to the row operation direction.
void row_op(StabilizerTableau& tableau,
            size_t row1,
            size_t row2,
            CliffordOperatorString& cx_ops) {
    add_cx(tableau, row2, row1, cx_ops);
}

void eliminate_chunk(StabilizerTableau& tableau,
                     size_t chunk_begin,
                     size_t chunk_end,
                     CliffordOperatorString& cx_ops,
                     bool up_to_down) {
    auto const n_qubits   = tableau.n_qubits();
    auto const chunk_size = chunk_end - chunk_begin;

    auto const bitset_hash = [](sul::dynamic_bitset<> const& bitset) {
        size_t hash = 0;
        for (size_t i = 0; i < bitset.num_blocks(); ++i) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            hash ^= std::hash<uint64_t>{}(bitset.data()[i]);
        }
        return hash;
    };

    // record visited chunks and the row index of the first occurrence
    std::unordered_map<sul::dynamic_bitset<>, size_t, decltype(bitset_hash)>
        visited_chunks{0, bitset_hash};

    auto const make_chunk = [&](size_t row) {
        sul::dynamic_bitset<> chunk(chunk_size);
        for (auto i : std::views::iota(chunk_begin, chunk_end)) {
            chunk.set(i - chunk_begin, tableau.stabilizer(i).is_x_set(row));
        }
        return chunk;
    };

    if (up_to_down) {
        for (auto row : std::views::iota(chunk_begin, n_qubits)) {
            auto const chunk = make_chunk(row);
            if (chunk.count() == 0) continue;

            if (visited_chunks.contains(chunk)) {
                // synthesize the chunk
                row_op(tableau, visited_chunks.at(chunk), row, cx_ops);
            } else {
                visited_chunks.emplace(chunk, row);
            }
        }
    } else {
        for (auto row : std::views::iota(0ul, chunk_end) |
                            std::views::reverse) {
            auto const chunk = make_chunk(row);
            if (chunk.count() == 0) continue;
            if (visited_chunks.contains(chunk)) {
                // synthesize the chunk
                row_op(tableau, visited_chunks.at(chunk), row, cx_ops);
            } else {
                visited_chunks.emplace(chunk, row);
            }
        }
    }
}

void make_main_diag_one(StabilizerTableau& tableau,
                        size_t col,
                        CliffordOperatorString& cx_ops,
                        bool up_to_down) {
    if (tableau.stabilizer(col).is_z_set(col)) {
        return;
    }
    auto const n_qubits = tableau.n_qubits();
    if (up_to_down) {
        for (auto row : std::views::iota(col + 1, n_qubits)) {
            if (tableau.stabilizer(col).is_z_set(row)) {
                row_op(tableau, row, col, cx_ops);
                break;
            }
        }
    } else {
        for (auto row : std::views::iota(0ul, col) | std::views::reverse) {
            if (tableau.stabilizer(col).is_z_set(row)) {
                row_op(tableau, row, col, cx_ops);
                break;
            }
        }
    }
}

void make_off_diag_zero(StabilizerTableau& tableau,
                        size_t col,
                        CliffordOperatorString& cx_ops,
                        bool up_to_down) {
    auto const n_qubits = tableau.n_qubits();
    if (up_to_down) {
        for (auto row : std::views::iota(col + 1, n_qubits)) {
            if (tableau.stabilizer(col).is_z_set(row)) {
                row_op(tableau, col, row, cx_ops);
            }
        }
    } else {
        for (auto row : std::views::iota(0ul, col) | std::views::reverse) {
            if (tableau.stabilizer(col).is_z_set(row)) {
                row_op(tableau, col, row, cx_ops);
            }
        }
    }
}

void eliminate_remaining(StabilizerTableau& tableau,
                         size_t chunk_begin,
                         size_t chunk_end,
                         CliffordOperatorString& cx_ops,
                         bool up_to_down) {
    auto const n_qubits = tableau.n_qubits();

    // perform Gaussian elimination in the chunk
    if (up_to_down) {
        for (auto col : std::views::iota(chunk_begin, chunk_end)) {
            make_main_diag_one(tableau, col, cx_ops, up_to_down);
            make_off_diag_zero(tableau, col, cx_ops, up_to_down);
        }
    } else {
        for (auto col : std::views::iota(chunk_begin, chunk_end) |
                            std::views::reverse) {
            make_main_diag_one(tableau, col, cx_ops, up_to_down);
            make_off_diag_zero(tableau, col, cx_ops, up_to_down);
        }
    }
}

}  // namespace
/**
 * @brief Synthesize the CX circuit using the Patel-Maslov-Hayes method.
 *        This method produces asymptotically optimal CX circuits.
 *
 * @param tableau The tableau to synthesize. Assumes the tableau is a
 *        pure CX circuit. If not, the behavior is undefined.
 * @param chunk_size The size of the chunk to use for the synthesis.
 *                   If not provided, the default chunk size is used.
 * @return CliffordOperatorString
 */
CliffordOperatorString
synthesize_cx_pmh(StabilizerTableau tableau,
                  std::optional<size_t> chunk_size) {
    // if chunk_size is not provided, use the default chunk size
    DVLAB_ASSERT(
        chunk_size != 0,
        "Chunk size must be greater than 0");

    auto const n_qubits = tableau.n_qubits();
    if (!chunk_size) {
        chunk_size = std::round(
            0.5 * std::log2(static_cast<double>(n_qubits)));
        if (*chunk_size == 0) {
            *chunk_size = 1;
        }
    }

    auto const n_chunks =
        std::ceil(static_cast<double>(n_qubits) /
                  static_cast<double>(*chunk_size));

    CliffordOperatorString cx_ops;

    // eliminate lower triangular part
    for (auto chunk_idx : std::views::iota(0ul, n_chunks)) {
        auto const chunk_begin =
            chunk_idx * *chunk_size;
        auto const chunk_end =
            std::min(chunk_begin + *chunk_size, n_qubits);
        if (chunk_size > 1) {
            eliminate_chunk(tableau, chunk_begin, chunk_end, cx_ops, true);
        }
        eliminate_remaining(tableau, chunk_begin, chunk_end, cx_ops, true);
    }

    for (auto chunk_idx : std::views::iota(0ul, n_chunks) |
                              std::views::reverse) {
        auto const chunk_begin =
            chunk_idx * *chunk_size;
        auto const chunk_end =
            std::min(chunk_begin + *chunk_size, n_qubits);
        if (chunk_size > 1) {
            eliminate_chunk(tableau, chunk_begin, chunk_end, cx_ops, false);
        }
        eliminate_remaining(tableau, chunk_begin, chunk_end, cx_ops, false);
    }

    return adjoint(cx_ops);
}

/**
 * @brief Synthesize a CX circuit using the Gaussian elimination method.
 *
 * @param tableau
 * @return CliffordOperatorString
 */
CliffordOperatorString
synthesize_cx_gaussian(StabilizerTableau const& tableau) {
    CliffordOperatorString cx_ops;

    auto copy = tableau;

    auto const n_qubits = tableau.n_qubits();

    // eliminate lower triangular part
    for (auto i : std::views::iota(0ul, n_qubits)) {
        eliminate_remaining(copy, i, i + 1, cx_ops, true);
    }

    for (auto i : std::views::iota(0ul, n_qubits) |
                      std::views::reverse) {
        eliminate_remaining(copy, i, i + 1, cx_ops, false);
    }

    return adjoint(cx_ops);
}

/**
 * @brief Synthesize a CX circuit using the PMH method with
 *        an exhaustive search on the chunk size.
 *
 * @param tableau
 * @return CliffordOperatorString
 */
CliffordOperatorString
synthesize_cx_pmh_exhaustive(StabilizerTableau const& tableau) {
    auto curr_best_cxs = CliffordOperatorString{};
    auto best_cx_count = std::numeric_limits<size_t>::max();
    auto chunk_size    = SIZE_MAX;
    for (auto i : std::views::iota(1ul, tableau.n_qubits() + 1)) {
        auto cxs = synthesize_cx_pmh(tableau, i);
        if (cxs.size() < best_cx_count) {
            curr_best_cxs = std::move(cxs);
            best_cx_count = cxs.size();
            chunk_size    = i;
        }
    }
    return curr_best_cxs;
}

namespace {
CliffordOperatorString
resynthesize_cxs(size_t n_qubits, CliffordOperatorString const& cxs) {
    auto tableau = StabilizerTableau(n_qubits);
    for (auto const& cx : cxs) {
        tableau.apply(cx);
    }
    return synthesize_cx_gaussian(tableau);
}

std::pair<CliffordOperatorString, std::vector<size_t>>
find_upper_and_diag(StabilizerTableau tableau) {
    auto const n_qubits = tableau.n_qubits();

    // upper triangular matrix accessor
    auto const u = [&](size_t row, size_t col) {
        return tableau.stabilizer(col).is_z_set(row);
    };

    // symmetric matrix accessor
    auto const s = [&](size_t row, size_t col) {
        return tableau.destabilizer(col).is_z_set(row);
    };

    for (auto a : std::views::iota(0ul, n_qubits) | std::views::reverse) {
        for (auto b : std::views::iota(a + 1, n_qubits) | std::views::reverse) {
            bool sum = 0;
            for (auto c : std::views::iota(b + 1, n_qubits)) {
                sum ^= u(a, c) & u(b, c);
            }
            // we don't bother setting destab_x because
            // gaussian elimination only use stab_z
            tableau.stabilizer(b).set_z(a, sum ^ s(a, b));
        }
    }

    auto diag_idx = std::vector<size_t>{};
    for (auto i : std::views::iota(0ul, n_qubits)) {
        bool row_sum = 0;
        for (auto j : std::views::iota(0ul, n_qubits)) {
            row_sum ^= u(i, j);
        }
        row_sum ^= s(i, i);
        if (row_sum) {
            diag_idx.push_back(i);
        }
    }

    auto const upper = synthesize_cx_gaussian(tableau);

    return {upper, diag_idx};
}

}  // namespace

/**
 * @brief Synthesize a H-free circuit using the Maslov-Roetteler method.
 *
 * @param tableau
 * @return CliffordOperatorString
 */
CliffordOperatorString
synthesize_h_free_mr(StabilizerTableau tableau) {
    auto ops = synthesize_cx_gaussian(tableau);

    // cancel out these CXs
    for (auto const& cx : ops | std::views::reverse) {
        tableau.apply(cx);
    }

    // if the circuit is Hadamard-free, the tableau should be in the form of
    // [I B]
    // [0 I]
    // where B is a symmetric matrix.

    // at this point, if the tableau is identity, we can return right away
    if (tableau.is_identity()) {
        return ops;
    }

    adjoint_inplace(ops);

    // else, we will decompose B = U (U^T) + D, where U is upper triangular
    // and D is diagonal.
    // This allows us to decompose the tableau into
    // [U O       ] [I  I] [U^-1 O  ] [I  D]
    // [O (U^T)^-1] [O  I] [O    U^T] [O  I]
    // first, we need to find the upper triangular matrix U

    auto [upper, diag_idx] = find_upper_and_diag(tableau);
    auto const n_qubits    = tableau.n_qubits();

    // reduce the tableau until only Pauli gates are left

    // cancel [U O       ]
    //        [O (U^T)^-1]
    for (auto const& cx : upper | std::views::reverse) {
        add_cx(tableau, cx.second[0], cx.second[1], ops);
    }

    ops = resynthesize_cxs(n_qubits, ops);

    // cancel [I  I]
    //        [O  I]
    for (auto i : std::views::iota(0ul, n_qubits)) {
        add_s(tableau, i, ops);
    }

    // cancel [U^-1 O  ]
    //        [O    U^T]
    for (auto const& cx : upper) {
        add_cx(tableau, cx.second[0], cx.second[1], ops);
    }

    // cancel [I  D]
    //        [O  I]
    for (auto const& idx : diag_idx) {
        add_s(tableau, idx, ops);
    }

    handle_negatives(tableau, ops);

    return adjoint(ops);
}

}  // namespace qsyn::tableau
