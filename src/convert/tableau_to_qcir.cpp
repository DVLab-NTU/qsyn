/****************************************************************************
  PackageName  [ tableau ]
  Synopsis     [ Define pauli rotation class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./tableau_to_qcir.hpp"

#include <gsl/narrow>
#include <random>
#include <stack>
#include <tl/adjacent.hpp>
#include <tl/enumerate.hpp>
#include <tl/to.hpp>

#include <cmath>
#include <complex>
#include <set>
#include <unordered_map>

#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "util/graph/digraph.hpp"
#include "util/graph/minimum_spanning_arborescence.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::experimental {

namespace {

void add_clifford_gate(qcir::QCir& qcir, CliffordOperator const& op) {
    using COT                  = CliffordOperatorType;
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

// NCF-only Clifford emission: restrict to {h,s,sdg,cx} by decomposing other Cliffords.
void add_clifford_gate_ncf(qcir::QCir& qcir, CliffordOperator const& op) {
    using COT                  = CliffordOperatorType;
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
            // Decompose V into {h,s,sdg}: V = H S H
            qcir.append(qcir::HGate(), {qubits[0]});
            qcir.append(qcir::SGate(), {qubits[0]});
            qcir.append(qcir::HGate(), {qubits[0]});
            break;
        case COT::vdg:
            // V† = H S† H
            qcir.append(qcir::HGate(), {qubits[0]});
            qcir.append(qcir::SdgGate(), {qubits[0]});
            qcir.append(qcir::HGate(), {qubits[0]});
            break;
        case COT::x:
            // X = H Z H, and Z = S S
            qcir.append(qcir::HGate(), {qubits[0]});
            qcir.append(qcir::SGate(), {qubits[0]});
            qcir.append(qcir::SGate(), {qubits[0]});
            qcir.append(qcir::HGate(), {qubits[0]});
            break;
        case COT::y:
            // Y = S X S†
            qcir.append(qcir::SGate(), {qubits[0]});
            add_clifford_gate_ncf(qcir, {COT::x, qubits});
            qcir.append(qcir::SdgGate(), {qubits[0]});
            break;
        case COT::z:
            // Z = S S
            qcir.append(qcir::SGate(), {qubits[0]});
            qcir.append(qcir::SGate(), {qubits[0]});
            break;
        case COT::cz:
            // CZ = H(t) CX(c,t) H(t)
            qcir.append(qcir::HGate(), {qubits[1]});
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            qcir.append(qcir::HGate(), {qubits[1]});
            break;
        case COT::swap:
            // SWAP = CX(a,b) CX(b,a) CX(a,b)
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            qcir.append(qcir::CXGate(), {qubits[1], qubits[0]});
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            break;
        case COT::ecr:
            // ecr(control,target) := cx; s(control); x(control); v(target)
            qcir.append(qcir::CXGate(), {qubits[0], qubits[1]});
            qcir.append(qcir::SGate(), {qubits[0]});
            add_clifford_gate_ncf(qcir, {COT::x, qubits});
            add_clifford_gate_ncf(qcir, {COT::v, std::array<size_t, 2>{qubits[1], 0}});
            break;
    }
}
}  // namespace

/**
 * @brief convert a stabilizer tableau to a QCir.
 *
 * @param clifford - pass by value on purpose
 * @return std::optional<qcir::QCir>
 */
namespace {
std::optional<qcir::QCir> to_qcir_clifford(StabilizerTableau const& clifford, StabilizerTableauSynthesisStrategy const& strategy, bool ncf_restricted_basis) {
    qcir::QCir qcir{clifford.n_qubits()};
    for (auto const& op : extract_clifford_operators(clifford, strategy)) {
        if (stop_requested()) {
            return std::nullopt;
        }
        if (ncf_restricted_basis) {
            add_clifford_gate_ncf(qcir, op);
        } else {
            add_clifford_gate(qcir, op);
        }
    }

    // Best-effort local shortening for NCF Clifford parts in the restricted basis.
    // This does NOT guarantee global optimality, but removes obvious redundancies:
    // - H H -> I
    // - CX CX -> I (same ordered pair)
    // - Reduce consecutive S/Sdg runs mod 4: S^0=I, S^1=S, S^2=SS, S^3=Sdg
    if (!ncf_restricted_basis) return qcir;

    // We'll simplify into a small stack of (repr, qubits) to avoid relying on gate IDs/types.
    // repr is one of: "h", "s", "sdg", "cx" in this restricted path.
    std::vector<std::pair<std::string, qsyn::QubitIdList>> stack;

    auto const emit = [&](std::string repr, qsyn::QubitIdList const& qs) {
        // cancel adjacent identical H / CX
        if (!stack.empty() && stack.back().first == repr && stack.back().second == qs &&
            (repr == "h" || repr == "cx")) {
            stack.pop_back();
            return;
        }
        stack.emplace_back(std::move(repr), qs);
    };

    auto const flush_s_power = [&](size_t qubit, int power_mod4) {
        int p = ((power_mod4 % 4) + 4) % 4;
        if (p == 0) return;
        if (p == 1) emit("s", qsyn::QubitIdList{qubit});
        else if (p == 2) {
            emit("s", qsyn::QubitIdList{qubit});
            emit("s", qsyn::QubitIdList{qubit});
        } else {  // 3
            emit("sdg", qsyn::QubitIdList{qubit});
        }
    };

    // Track pending consecutive S/Sdg on each qubit.
    std::vector<int> pending_s(clifford.n_qubits(), 0);

    auto const flush_all_pending = [&]() {
        for (size_t q = 0; q < pending_s.size(); ++q) {
            if (pending_s[q] != 0) {
                flush_s_power(q, pending_s[q]);
                pending_s[q] = 0;
            }
        }
    };

    for (auto const* g : qcir.get_gates()) {
        auto const& op = g->get_operation();
        auto const qs  = g->get_qubits();
        auto const repr = op.get_repr();

        if (repr == "s") {
            pending_s[qs[0]] += 1;
            continue;
        }
        if (repr == "sdg") {
            pending_s[qs[0]] -= 1;
            continue;
        }

        // Before emitting a non-(S/Sdg) gate, flush pending S-power on the touched qubits
        // to preserve order.
        if (qs.size() == 1) {
            if (pending_s[qs[0]] != 0) {
                flush_s_power(qs[0], pending_s[qs[0]]);
                pending_s[qs[0]] = 0;
            }
        } else if (qs.size() == 2) {
            for (auto q : qs) {
                if (pending_s[q] != 0) {
                    flush_s_power(q, pending_s[q]);
                    pending_s[q] = 0;
                }
            }
        } else {
            flush_all_pending();
        }

        // Emit remaining restricted Cliffords.
        if (repr == "h" || repr == "cx") {
            emit(repr, qs);
        } else {
            // Shouldn't happen in restricted basis, but be safe: treat as barrier for S aggregation.
            flush_all_pending();
            emit(repr, qs);
        }
    }

    // Flush remaining S/Sdg at end.
    flush_all_pending();

    // Build a QCir back from the stack.
    auto simplified = qcir::QCir{clifford.n_qubits()};
    for (auto const& [r, qs] : stack) {
        if (r == "h") {
            simplified.append(qcir::HGate(), qs);
        } else if (r == "s") {
            simplified.append(qcir::SGate(), qs);
        } else if (r == "sdg") {
            simplified.append(qcir::SdgGate(), qs);
        } else if (r == "cx") {
            simplified.append(qcir::CXGate(), qs);
        }
    }

    return simplified;
}
}  // namespace

std::optional<qcir::QCir> to_qcir(StabilizerTableau const& clifford, StabilizerTableauSynthesisStrategy const& strategy) {
    return to_qcir_clifford(clifford, strategy, false);
}

std::optional<qcir::QCir> NaivePauliRotationsSynthesisStrategy::synthesize(std::vector<PauliRotation> const& rotations) const {
    if (rotations.empty()) {
        return qcir::QCir{0};
    }

    auto qcir = qcir::QCir{rotations.front().n_qubits()};

    for (auto const& rotation : rotations) {
        auto [ops, qubit] = extract_clifford_operators(rotation);

        for (auto const& op : ops) {
            add_clifford_gate(qcir, op);
        }

        qcir.append(qcir::PZGate(rotation.phase()), {qubit});

        adjoint_inplace(ops);

        for (auto const& op : ops) {
            add_clifford_gate(qcir, op);
        }
    }

    return qcir;
}

std::optional<qcir::QCir> TParPauliRotationsSynthesisStrategy::synthesize(std::vector<PauliRotation> const& /* rotations */) const {
    spdlog::error("TPar Synthesis Strategy is not implemented yet!!");
    return std::nullopt;
}

namespace {

// Return the single qubit index if all rotations act on exactly that one qubit; else nullopt.
std::optional<size_t> ncf_single_qubit_support(std::vector<PauliRotation> const& rotations) {
    if (rotations.empty()) return std::nullopt;
    auto const n_qubits = rotations.front().n_qubits();
    std::optional<size_t> common_qubit;
    for (auto const& r : rotations) {
        size_t count = 0;
        size_t q     = 0;
        for (size_t i = 0; i < n_qubits; ++i) {
            if (r.get_pauli_type(i) != Pauli::i) {
                ++count;
                q = i;
            }
        }
        if (count != 1) return std::nullopt;
        if (!common_qubit) common_qubit = q;
        else if (*common_qubit != q)
            return std::nullopt;
    }
    return common_qubit;
}

// Max Pauli weight over the block; used to allow ≤2-local NCF-2q emission as Clifford+RZ.
size_t ncf_block_support_width(std::vector<PauliRotation> const& rotations) {
    if (rotations.empty()) return 0;
    auto const n_qubits = rotations.front().n_qubits();
    size_t width        = 0;
    for (size_t q = 0; q < n_qubits; ++q) {
        for (auto const& r : rotations) {
            if (r.get_pauli_type(q) != Pauli::i) {
                ++width;
                break;
            }
        }
    }
    return width;
}

/**
 * @brief Canonical forward Clifford shell C for NCF export: S(ancilla) then sorted CNOT ladder.
 *        Single-qubit H/S on the fold pivot are omitted (emitted in the inner rotation block).
 */
CliffordOperatorString ncf_canonicalize_forward_clifford(
    CliffordOperatorString const& ops,
    size_t pivot) {
    using COT = CliffordOperatorType;

    std::vector<std::pair<size_t, size_t>> cx_edges;
    std::unordered_map<size_t, int> non_pivot_s_phase;

    for (auto const& [type, qubits] : ops) {
        switch (type) {
            case COT::cx:
                cx_edges.emplace_back(qubits[0], qubits[1]);
                break;
            case COT::s:
                if (qubits[0] != pivot) {
                    ++non_pivot_s_phase[qubits[0]];
                }
                break;
            case COT::sdg:
                if (qubits[0] != pivot) {
                    --non_pivot_s_phase[qubits[0]];
                }
                break;
            default:
                break;
        }
    }

    std::set<std::pair<size_t, size_t>> seen;
    std::vector<std::pair<size_t, size_t>> unique_cx;
    for (auto const& edge : cx_edges) {
        if (seen.insert(edge).second) {
            unique_cx.push_back(edge);
        }
    }
    std::ranges::sort(unique_cx);

    std::optional<size_t> ancilla;
    for (auto const& [q, phase] : non_pivot_s_phase) {
        if (phase % 2 != 0) {
            ancilla = q;
            break;
        }
    }
    if (!ancilla && unique_cx.size() > 1) {
        bool star_from_pivot = std::ranges::all_of(unique_cx, [&](auto const& edge) {
            return edge.first == pivot;
        });
        if (star_from_pivot) {
            ancilla = std::ranges::max_element(unique_cx, {}, &std::pair<size_t, size_t>::second)->second;
        }
    }

    CliffordOperatorString shell;
    if (ancilla) {
        shell.emplace_back(COT::s, std::array<size_t, 2>{*ancilla, 0});
    }
    for (auto const& [control, target] : unique_cx) {
        shell.emplace_back(COT::cx, std::array<size_t, 2>{control, target});
    }
    return shell;
}

// exp(i theta * P) as 2x2 matrix for P in {X,Y,Z}. Convention: rotation is exp(i theta P).
void pauli_exp_matrix(Pauli P, double theta, std::complex<double> out[2][2]) {
    using namespace std::complex_literals;
    double c = std::cos(theta), s = std::sin(theta);
    if (P == Pauli::z) {
        out[0][0] = std::exp(1i * theta);
        out[0][1] = 0;
        out[1][0] = 0;
        out[1][1] = std::exp(-1i * theta);
        return;
    }
    if (P == Pauli::x) {
        out[0][0] = c;
        out[0][1] = 1i * s;
        out[1][0] = 1i * s;
        out[1][1] = c;
        return;
    }
    if (P == Pauli::y) {
        out[0][0] = c;
        out[0][1] = s;
        out[1][0] = -s;
        out[1][1] = c;
        return;
    }
    out[0][0] = 1;
    out[0][1] = 0;
    out[1][0] = 0;
    out[1][1] = 1;
}

void mat2_mul(std::complex<double> const a[2][2], std::complex<double> const b[2][2], std::complex<double> out[2][2]) {
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j) {
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j];
        }
}

// ZYZ decomposition: U = Rz(phi) * Ry(theta) * Rz(lambda). Rz(t)=exp(-i t Z/2), Ry(t)=exp(-i t Y/2).
// Returns (phi, theta, lambda) in radians. Assumes U is special unitary (det=1).
void zyz_decompose(std::complex<double> const u[2][2], double& phi, double& theta, double& lambda) {
    using namespace std::complex_literals;
    double const tol = 1e-10;
    double c_half   = std::abs(u[0][0]);
    double s_half   = std::abs(u[0][1]);
    if (c_half < tol && s_half < tol) {
        theta = 0;
        phi   = 0;
        lambda = std::arg(u[1][0]) + std::numbers::pi / 2;
        return;
    }
    theta = 2.0 * std::atan2(s_half, c_half);
    double phi_plus_lambda = 2.0 * std::arg(u[0][0]);
    double phi_minus_lambda = -2.0 * std::arg(u[0][1]) - 2.0 * std::numbers::pi;
    phi    = (phi_plus_lambda + phi_minus_lambda) / 2.0;
    lambda = (phi_plus_lambda - phi_minus_lambda) / 2.0;
}

}  // namespace

std::optional<qcir::QCir> NcfMergePauliRotationsSynthesisStrategy::synthesize(std::vector<PauliRotation> const& rotations) const {
    if (rotations.empty()) return qcir::QCir{0};
    auto const qubit_opt = ncf_single_qubit_support(rotations);
    if (qubit_opt) {
        size_t const qubit = *qubit_opt;
        qcir::QCir qcir(rotations.front().n_qubits());
        for (size_t ri = 0; ri < rotations.size(); ++ri) {
            auto const& r = rotations[ri];
            // After tableau optimize ncf, rotations in a group are already conjugated to a single qubit.
            // Emit them explicitly without merging/decomposing:
            //   Z: rz(θ)
            //   X: h; rz(θ); h
            //   Y: sdg; h; rz(θ); h; s
            //
            // Anti-pair blocks map the second anticommuting term to X on the pivot, but the
            // hyperbolic gadget is the Y chain S†·H·Rz·H·S (see NCF paper / debug canonical form).
            auto emit_pauli = [&](Pauli p) {
                if (p == Pauli::x) {
                    qcir.append(qcir::HGate(), qsyn::QubitIdList{qubit});
                    qcir.append(qcir::RZGate(r.phase()), qsyn::QubitIdList{qubit});
                    qcir.append(qcir::HGate(), qsyn::QubitIdList{qubit});
                } else if (p == Pauli::y) {
                    qcir.append(qcir::SdgGate(), qsyn::QubitIdList{qubit});
                    qcir.append(qcir::HGate(), qsyn::QubitIdList{qubit});
                    qcir.append(qcir::RZGate(r.phase()), qsyn::QubitIdList{qubit});
                    qcir.append(qcir::HGate(), qsyn::QubitIdList{qubit});
                    qcir.append(qcir::SGate(), qsyn::QubitIdList{qubit});
                } else if (p == Pauli::z) {
                    qcir.append(qcir::RZGate(r.phase()), qsyn::QubitIdList{qubit});
                }
            };

            auto p = r.get_pauli_type(qubit);
            if (ri == 1 && rotations.size() == 2 &&
                rotations[0].get_pauli_type(qubit) == Pauli::z && p == Pauli::x) {
                emit_pauli(Pauli::y);
            } else {
                emit_pauli(p);
            }
        }
        return qcir;
    }

    // 2-qubit NCF: conjugated block supported on ≤2 qubits. Emit each Pauli as a
    // Clifford sandwich + rz (no fused 2q unitary, no Clifford+T synthesis).
    if (ncf_block_support_width(rotations) <= 2) {
        qcir::QCir qcir(rotations.front().n_qubits());
        for (auto const& r : rotations) {
            auto [ops, qubit] = extract_clifford_operators(r);
            for (auto const& op : ops) {
                add_clifford_gate(qcir, op);
            }
            qcir.append(qcir::RZGate(r.phase()), {qubit});
            adjoint_inplace(ops);
            for (auto const& op : ops) {
                add_clifford_gate(qcir, op);
            }
        }
        return qcir;
    }

    return NaivePauliRotationsSynthesisStrategy{}.synthesize(rotations);
}

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
    GraySynthPauliRotationsSynthesisStrategy::Mode mode,
    std::vector<PauliRotation>& rotations,
    qcir::QCir& qcir,
    StabilizerTableau& final_clifford,
    std::unordered_set<std::size_t> const& frozen_rotations,
    std::size_t num_rotations,
    std::vector<std::size_t> const& random_order) {
    using Mode = GraySynthPauliRotationsSynthesisStrategy::Mode;

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
get_cofactor_row(std::vector<PauliRotation> const& rotations, std::vector<size_t> const& rotation_filter, std::vector<size_t> const& qubit_filter) {
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

std::optional<qcir::QCir>
GraySynthPauliRotationsSynthesisStrategy::synthesize(
    std::vector<PauliRotation> const& rotations) const {
    auto const num_qubits    = rotations.front().n_qubits();
    auto const num_rotations = rotations.size();

    if (num_qubits == 0) {
        return qcir::QCir{0};
    }

    if (num_rotations == 0) {
        return qcir::QCir{num_qubits};
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
        auto const [rotation_filter, qubit_filter, targ] = std::move(stack.back());
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

    auto const final_clifford_circ = to_qcir(
        final_clifford,
        AGSynthesisStrategy{});

    if (!final_clifford_circ) {
        return std::nullopt;
    }
    qcir.compose(*final_clifford_circ);

    return qcir;
}

namespace {

size_t hamming_weight(
    PauliRotation const& rotation) {
    auto const num_qubits = rotation.n_qubits();
    auto num_ones         = 0ul;
    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (rotation.pauli_product().is_z_set(i)) {
            num_ones++;
        }
    }
    return num_ones;
}

// get the index of the rotation with the minimum number of 1s
// A term of k ones can always be synthesized with k-1 CNOTs
size_t get_best_rotation_idx(std::vector<PauliRotation> const& rotations) {
    auto min_ones = SIZE_MAX;
    auto best_idx = SIZE_MAX;
    for (auto const& [idx, rotation] : tl::views::enumerate(rotations)) {
        auto const num_ones = hamming_weight(rotation);
        if (num_ones < min_ones) {
            min_ones = num_ones;
            best_idx = idx;
        }
    }
    return best_idx;
}

size_t hamming_weight(
    std::vector<PauliRotation> const& rotations,
    size_t q_idx) {
    return std::ranges::count_if(rotations, [&](auto const& rotation) {
        return rotation.pauli_product().is_z_set(q_idx);
    });
}

size_t hamming_distance(
    std::vector<PauliRotation> const& rotations,
    size_t q1_idx,
    size_t q2_idx) {
    return std::ranges::count_if(rotations, [&](auto const& rotation) {
        return rotation.pauli_product().is_z_set(q1_idx) !=
               rotation.pauli_product().is_z_set(q2_idx);
    });
}

dvlab::Digraph<size_t, int> get_parity_graph(
    std::vector<PauliRotation> const& rotations,
    PauliRotation const& target_rotation) {
    auto const num_qubits = rotations.front().n_qubits();

    auto g = dvlab::Digraph<size_t, int>{};

    auto qubit_vec = std::vector<size_t>{};

    for (auto i : std::views::iota(0ul, num_qubits)) {
        if (target_rotation.pauli_product().is_z_set(i)) {
            g.add_vertex_with_id(i);
            qubit_vec.push_back(i);
        }
    }

    for (auto const& [i, j] : dvlab::combinations<2>(qubit_vec)) {
        auto const dist =
            gsl::narrow_cast<int>(hamming_distance(rotations, i, j));
        auto const weight_i =
            gsl::narrow_cast<int>(hamming_weight(rotations, i));
        auto const weight_j =
            gsl::narrow_cast<int>(hamming_weight(rotations, j));
        g.add_edge(i, j, dist - weight_j - 1);
        g.add_edge(j, i, dist - weight_i - 1);
    }

    return g;
}

}  // namespace

std::optional<qcir::QCir>
MstSynthesisStrategy::synthesize(
    std::vector<PauliRotation> const& rotations) const {
    auto const num_qubits    = rotations.front().n_qubits();
    auto const num_rotations = rotations.size();

    if (num_qubits == 0) {
        return qcir::QCir{0};
    }

    if (num_rotations == 0) {
        return qcir::QCir{num_qubits};
    }

    auto copy_rotations = rotations;

    auto qcir = qcir::QCir{copy_rotations.front().n_qubits()};

    StabilizerTableau final_clifford{num_qubits};

    auto const add_cx = [&](size_t ctrl, size_t targ) {
        for (auto& rot : copy_rotations) {
            rot.cx(ctrl, targ);
        }
        qcir.append(qcir::CXGate(), {ctrl, targ});
        final_clifford.prepend_cx(ctrl, targ);
    };

    // checks if all rotations are diagonal
    if (!std::ranges::all_of(rotations, &PauliRotation::is_diagonal)) {
        spdlog::error("MST only supports diagonal rotations");
        return std::nullopt;
    }

    while (!copy_rotations.empty()) {
        auto const best_rotation_idx = get_best_rotation_idx(copy_rotations);
        std::swap(copy_rotations[best_rotation_idx], copy_rotations.back());
        auto const best_rotation = std::move(copy_rotations.back());
        copy_rotations.pop_back();

        auto const parity_graph =
            get_parity_graph(copy_rotations, best_rotation);

        auto const [mst, root] =
            dvlab::minimum_spanning_arborescence(parity_graph);

        // post-order traversal to add CXs
        std::stack<size_t> stack;
        std::vector<size_t> post_order_rev;

        stack.push(root);

        while (!stack.empty()) {
            auto const v = stack.top();
            stack.pop();
            post_order_rev.push_back(v);

            for (auto const& n : mst.out_neighbors(v)) {
                stack.push(n);
            }
        }

        while (!post_order_rev.empty()) {
            auto const v = post_order_rev.back();
            post_order_rev.pop_back();

            // get the predecessor of v

            if (mst.in_degree(v) == 1) {
                auto const pred = *mst.in_neighbors(v).begin();
                add_cx(v, pred);
            } else {
                DVLAB_ASSERT(
                    mst.in_degree(v) == 0 && v == root,
                    "The node with no incoming edges should be the root");
            }
        }

        // add the rotation at the root
        qcir.append(qcir::PZGate(best_rotation.phase()), {root});
    }

    // synthesize the final clifford

    auto const final_clifford_circ = to_qcir(
        final_clifford,
        AGSynthesisStrategy{});

    if (!final_clifford_circ) {
        return std::nullopt;
    }
    qcir.compose(*final_clifford_circ);

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

/**
 * @brief Return true if tableau has NCF shape: [Stab][C†][R][C][C†][R][C]...
 *        (first block StabilizerTableau, then repeating triplets C†, rotations, C).
 */
bool has_ncf_canonical_shape(Tableau const& tableau) {
    size_t const n = tableau.size();
    if (n < 4) return false;
    if (!std::holds_alternative<StabilizerTableau>(tableau.front())) return false;
    if ((n - 1) % 3 != 0) return false;
    for (size_t i = 1; i < n; ++i) {
        bool want_clifford = (i % 3 == 1 || i % 3 == 0);  // 1,3,4,6,7,9 -> C†,C,C†,C,...
        bool want_rotations = (i % 3 == 2);               // 2,5,8 -> R
        bool is_clifford    = std::holds_alternative<StabilizerTableau>(tableau[i]);
        bool is_rotations   = std::holds_alternative<std::vector<PauliRotation>>(tableau[i]);
        if (want_rotations && !is_rotations) return false;
        if (want_clifford && !is_clifford) return false;
    }
    return true;
}

/**
 * @brief Emit QCir in tableau order for NCF blocks: [front][C†][R][C] repeated per group.
 *        This matches `tableau print` and the paper structure; it does NOT merge all rotations
 *        into one middle segment.
 */
std::optional<qcir::QCir> to_qcir_ncf_sequential(Tableau const& tableau, StabilizerTableauSynthesisStrategy const& st_strategy, PauliRotationsSynthesisStrategy const& pr_strategy) {
    size_t const n = tableau.size();
    qcir::QCir qcir{tableau.n_qubits()};

    auto const simplify_cancel_hh = [&](qcir::QCir const& in) -> qcir::QCir {
        // Cancel consecutive H on the same qubit if no intervening gate touches that qubit.
        // Gates on other qubits do not prevent cancellation.
        qcir::QCir out{in.get_num_qubits()};
        std::vector<bool> pending_h(in.get_num_qubits(), false);

        auto const flush_h = [&](qsyn::QubitIdType q) {
            if (pending_h[q]) {
                out.append(qcir::HGate(), qsyn::QubitIdList{q});
                pending_h[q] = false;
            }
        };

        std::vector<qcir::QCirGate const*> gate_list;
        if (in.preserve_append_order()) {
            gate_list = in.get_gates_in_append_order();
        } else {
            gate_list.assign(in.get_gates().begin(), in.get_gates().end());
        }

        for (auto const* g : gate_list) {
            auto const qs   = g->get_qubits();
            auto const repr = g->get_operation().get_repr();

            if (repr == "h" && qs.size() == 1) {
                auto const q = qs[0];
                pending_h[q] = !pending_h[q];  // H*H cancels
                continue;
            }

            // Flush pending H on touched qubits before emitting non-H.
            for (auto q : qs) {
                flush_h(q);
            }
            out.append(g->get_operation(), qs);
        }

        // Flush remaining pending H.
        for (qsyn::QubitIdType q = 0; q < pending_h.size(); ++q) {
            flush_h(q);
        }
        out.set_preserve_append_order(in.preserve_append_order());
        return out;
    };

    auto const emit_ops = [&](qcir::QCir& out, CliffordOperatorString const& ops) {
        for (auto const& op : ops) {
            add_clifford_gate_ncf(out, op);
        }
    };

    auto const clifford_block_to_qcir = [&](size_t idx) -> std::optional<qcir::QCir> {
        // Prefer direct ops emission if present (NCF direct-ops mode).
        if (auto const& maybe_ops = tableau.get_block_ops(idx); maybe_ops.has_value()) {
            qcir::QCir c{tableau.n_qubits()};

            // NCF blocks alternate [C†][R][C]. Emit symmetric shells: S(ancilla)+CX in C,
            // reverse CX + S†(ancilla) in C† (mirror of C).
            bool const is_ncf_c_dagger = idx >= 1 && (idx % 3 == 1);
            bool const is_ncf_c        = idx >= 3 && (idx % 3 == 0);
            if (is_ncf_c_dagger || is_ncf_c) {
                size_t const rot_idx = is_ncf_c_dagger ? idx + 1 : idx - 1;
                size_t const c_idx   = is_ncf_c ? idx : idx + 2;
                if (rot_idx < n && c_idx < n &&
                    std::holds_alternative<std::vector<PauliRotation>>(tableau[rot_idx]) &&
                    tableau.get_block_ops(c_idx).has_value()) {
                    auto const& rots        = std::get<std::vector<PauliRotation>>(tableau[rot_idx]);
                    auto const& forward_ops = *tableau.get_block_ops(c_idx);
                    if (auto const pivot = ncf_single_qubit_support(rots)) {
                        auto const shell = ncf_canonicalize_forward_clifford(forward_ops, *pivot);
                        emit_ops(c, is_ncf_c_dagger ? adjoint(shell) : shell);
                        return c;
                    }
                }
            }

            emit_ops(c, *maybe_ops);
            return c;
        }
        // Fallback: synthesize from stabilizer tableau.
        auto const& st = std::get<StabilizerTableau>(tableau[idx]);
        return to_qcir_clifford(st, st_strategy, true);
    };

    for (size_t i = 0; i < n; ++i) {
        if (stop_requested()) return std::nullopt;
        auto const frag =
            std::visit(
                dvlab::overloaded{
                    [&](StabilizerTableau const&) { return clifford_block_to_qcir(i); },
                    [&](std::vector<PauliRotation> const& pr) { return to_qcir(pr, pr_strategy); }},
                tableau[i]);
        if (!frag) return std::nullopt;
        qcir.compose(*frag);
    }
    qcir.set_preserve_append_order(true);
    return simplify_cancel_hh(qcir);
}

/**
 * @brief convert a stabilizer tableau and a list of Pauli rotations to a QCir.
 *        If the tableau has NCF block shape ([Stab][C†][R][C]...), emit in that same order
 *        (per-group C† R C), with NCF-restricted Clifford emission and optional stored ops.
 *
 * @param clifford
 * @param pauli_rotations
 * @return qcir::QCir
 */
std::optional<qcir::QCir> to_qcir(Tableau const& tableau, StabilizerTableauSynthesisStrategy const& st_strategy, PauliRotationsSynthesisStrategy const& pr_strategy) {
    if (has_ncf_canonical_shape(tableau)) {
        return to_qcir_ncf_sequential(tableau, st_strategy, pr_strategy);
    }

    qcir::QCir qcir{tableau.n_qubits()};

    for (auto const& subtableau : tableau) {
        if (stop_requested()) {
            return std::nullopt;
        }
        auto const qc_fragment =
            std::visit(
                dvlab::overloaded{
                    [&st_strategy](StabilizerTableau const& st) { return to_qcir(st, st_strategy); },
                    [&pr_strategy](std::vector<PauliRotation> const& pr) { return to_qcir(pr, pr_strategy); }},
                subtableau);
        if (!qc_fragment) {
            return std::nullopt;
        }
        qcir.compose(*qc_fragment);
    }

    return qcir;
}

}  // namespace qsyn::experimental
