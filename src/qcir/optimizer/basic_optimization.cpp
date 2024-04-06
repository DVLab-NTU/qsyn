/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

#include <ranges>

#include "../basic_gate_type.hpp"
#include "../qcir.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"
#include "fmt/core.h"
#include "qsyn/qsyn_type.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::qcir {

namespace {
void remove_last_occurrence_if(std::ranges::range auto& range, auto const& pred) {
    if (auto it = std::ranges::find_if(range | std::views::reverse, pred); it != range.rend()) {
        range.erase(std::prev(it.base()));
    }
}
}  // namespace

/**
 * @brief Parse the circuit and forward and backward iteratively and optimize it
 *
 * @param doSwap permute the qubit if true
 * @param separateCorrection separate corrections if true
 * @param maxIter
 * @return QCir* Optimized Circuit
 */
std::optional<QCir> Optimizer::basic_optimization(QCir const& qcir, BasicOptimizationConfig const& config) {
    reset(qcir);
    std::vector<size_t> orig_stats, prev_stats, stats;
    orig_stats = Optimizer::_compute_stats(qcir);
    spdlog::info("Start basic optimization");

    _iter = 0;
    // REVIEW - this is rather a weird logic
    //          I'm only restructuring the code here
    //          consider taking a look at why this is necessary
    QCir result = parse_forward(qcir, false, config);
    result      = parse_backward(result, false, config);
    result      = parse_forward(result, false, config);
    prev_stats  = Optimizer::_compute_stats(qcir);
    result      = parse_backward(result, true, config);
    stats       = Optimizer::_compute_stats(result);
    result      = parse_forward(result, true, config);

    while (!stop_requested() && _iter < config.maxIter &&
           (prev_stats[0] > stats[0] || prev_stats[1] > stats[1] || prev_stats[2] > stats[2])) {
        prev_stats = stats;

        result = parse_backward(result, true, config);
        stats  = Optimizer::_compute_stats(result);
        // TODO - Find a more efficient way
        result = parse_forward(result, true, config);
    }

    if (stop_requested()) {
        spdlog::warn("optimization interrupted");
        return std::nullopt;
    }

    spdlog::info("Basic optimization finished after {} iterations.", _iter * 2 + 1);
    spdlog::info("  Two-qubit gates: {} → {}", orig_stats[0], stats[0]);
    spdlog::info("  Hadamard gates : {} → {}", orig_stats[1], stats[1]);
    spdlog::info("  Non-Pauli gates: {} → {}", orig_stats[2], stats[2]);

    return result;
}

/**
 * @brief Parse through the gates according to topological order and optimize the circuit
 *
 * @return QCir* : new Circuit
 */
QCir Optimizer::_parse_once(QCir const& qcir, bool reversed, bool do_minimize_czs, BasicOptimizationConfig const& config) {
    (reversed)
        ? spdlog::debug("Start parsing backward")
        : spdlog::debug("Start parsing forward");

    reset(qcir);
    auto gates = qcir.get_gates();
    if (reversed) {
        std::reverse(gates.begin(), gates.end());
    }
    for (auto& gate : gates) {
        parse_gate(*gate, config.doSwap, do_minimize_czs);
    }

    // TODO - Find a method to avoid using parameter "erase"
    for (auto& t : _hadamards) {
        _add_hadamard(t, false);
    }

    for (auto& t : _zs) {
        _add_single_z_rotation_gate(t, dvlab::Phase(1));
    }

    QCir result = _build_from_storage(qcir.get_num_qubits(), reversed);
    result.set_filename(qcir.get_filename());
    result.add_procedures(qcir.get_procedures());

    for (auto& t : _xs) {
        auto gate = _storage[_store_x(t)];
        reversed
            ? result.prepend(gate)
            : result.append(gate);
    }

    _swaps = Optimizer::_get_swap_path();

    for (auto& [c, t] : _swaps) {
        // TODO - Use SWAP gate to replace cnots
        auto cnot1 = _storage[_store_cx(c, t)];
        auto cnot2 = _storage[_store_cx(t, c)];
        auto cnot3 = _storage[_store_cx(c, t)];
        if (reversed) {
            result.prepend(cnot1);
            result.prepend(cnot2);
            result.prepend(cnot3);
        } else {
            result.append(cnot1);
            result.append(cnot2);
            result.append(cnot3);
        }
    }
    std::string statistics_str;
    fmt::format_to(std::back_inserter(statistics_str), "  ParseForward No.{} iteration done.\n", _iter);
    fmt::format_to(std::back_inserter(statistics_str), "  Operated rule numbers in this forward are: \n");
    fmt::format_to(std::back_inserter(statistics_str), "    Fuse the Zphase: {}\n", _statistics.FUSE_PHASE);
    fmt::format_to(std::back_inserter(statistics_str), "    Fuse the Zphase: {}\n", _statistics.FUSE_PHASE);
    fmt::format_to(std::back_inserter(statistics_str), "    X gate canceled: {}\n", _statistics.X_CANCEL);
    fmt::format_to(std::back_inserter(statistics_str), "    H-S exchange   : {}\n", _statistics.HS_EXCHANGE);
    fmt::format_to(std::back_inserter(statistics_str), "    Cnot canceled  : {}\n", _statistics.CNOT_CANCEL);
    fmt::format_to(std::back_inserter(statistics_str), "    CZ canceled    : {}\n", _statistics.CZ_CANCEL);
    fmt::format_to(std::back_inserter(statistics_str), "    Crz transform  : {}\n", _statistics.CRZ_TRANSFORM);
    fmt::format_to(std::back_inserter(statistics_str), "    Do swap        : {}\n", _statistics.DO_SWAP);
    fmt::format_to(std::back_inserter(statistics_str), "  Note: {} CZs had been transformed into CXs.\n", _statistics.CZ2CX);
    fmt::format_to(std::back_inserter(statistics_str), "        {} CXs had been transformed into CZs.\n", _statistics.CX2CZ);
    fmt::format_to(std::back_inserter(statistics_str), "  Note: {} swap gates had been added in the swap path.\n", _swaps.size());
    if (config.printStatistics) {
        fmt::println("{}", statistics_str);
    }
    for (auto const& line : dvlab::str::views::split_to_string_views(statistics_str, "\n")) {
        spdlog::debug("{}", line);
    }
    spdlog::debug("");

    _iter++;

    return result;
}

/**
 * @brief Parse the gate
 *
 * @param gate
 * @return true : Successfully parse a gate,
 * @return false : Encounter a gate that is not supported.
 */
bool Optimizer::parse_gate(QCirGate& gate, bool do_swap, bool minimize_czs) {
    _permute_gates(gate);

    if (gate.get_operation() == HGate()) {
        _match_hadamards(gate);
        return true;
    }

    if (gate.get_operation() == XGate()) {
        _match_xs(gate);
        return true;
    }

    if (is_single_z_rotation(gate)) {
        _match_z_rotations(gate);
        return true;
    }

    if (gate.get_operation() == CZGate()) {
        _match_czs(gate, do_swap, minimize_czs);
    }

    if (gate.get_operation() == CXGate()) {
        _match_cxs(gate, do_swap, minimize_czs);
    }
    return false;
}

/**
 * @brief Add the gate from storage (_gates) to the circuit
 *
 * @param circuit
 */
QCir Optimizer::_build_from_storage(size_t n_qubits, bool reversed) {
    QCir circuit{n_qubits};

    while (std::ranges::any_of(_gates | std::views::values, [](auto const& gate_list) { return !gate_list.empty(); })) {
        dvlab::utils::ordered_hashset<size_t> available_ids;
        for (auto& [qubit, gate_list] : _gates) {
            while (!gate_list.empty()) {
                auto g = _storage[gate_list[0]];
                if (g.get_operation() != CXGate() && g.get_operation() != CZGate()) {
                    reversed
                        ? circuit.prepend(g)
                        : circuit.append(g);
                    gate_list.erase(gate_list.begin());
                    continue;
                }
                if (available_ids.contains(g.get_id())) {
                    available_ids.erase(g.get_id());
                    auto q2 = (qubit == g.get_qubit(0)) ? g.get_qubit(1) : g.get_qubit(0);
                    remove_last_occurrence_if(_gates[q2], [&](size_t id) { return g.get_id() == id; });
                    reversed
                        ? circuit.prepend(g)
                        : circuit.append(g);
                    gate_list.erase(gate_list.begin());
                    continue;
                }

                auto const type = g.get_operation() != CZGate() && g.get_qubit(0) != qubit;
                std::vector<size_t> removed;
                available_ids.emplace(g.get_id());
                for (size_t i = 1; i < gate_list.size(); i++) {
                    auto g2 = _storage[gate_list[i]];
                    if ((!type && is_single_z_rotation(g2)) || (type && is_single_x_rotation(g2))) {
                        reversed
                            ? circuit.prepend(g2)
                            : circuit.append(g2);
                        removed.emplace(removed.begin(), i);
                    } else if (g2.get_operation() != CXGate() && g2.get_operation() != CZGate()) {
                        break;
                    } else if ((!type && (g2.get_operation() == CZGate() || g2.get_qubit(0) == qubit)) ||
                               (type && (g2.get_operation() == CXGate() && g2.get_qubit(1) == qubit))) {
                        if (available_ids.contains(g2.get_id())) {
                            available_ids.erase(g2.get_id());
                            auto q2 = qubit == g2.get_qubit(0) ? g2.get_qubit(1) : g2.get_qubit(0);
                            remove_last_occurrence_if(_gates[q2], [&](size_t id) { return g2.get_id() == id; });
                            reversed
                                ? circuit.prepend(g2)
                                : circuit.append(g2);
                            removed.emplace(removed.begin(), i);
                        } else {
                            available_ids.emplace(g2.get_id());
                        }
                    } else {
                        break;
                    }
                }
                for (auto const i : removed) {
                    gate_list.erase(dvlab::iterator::next(gate_list.begin(), i));
                }
                break;
            }
        }
    }

    return circuit;
}

/**
 * @brief Add a Hadamard gate to the output. Called by `parseGate`
 *
 * @param target Index of the target qubit
 */
void Optimizer::_add_hadamard(QubitIdType target, bool erase) {
    auto h_gate = _store_h(target);
    _gates[target].emplace_back(h_gate);
    if (erase) _hadamards.erase(target);
    _available_gates[target].clear();
    _qubit_available[target] = false;
}

/**
 * @brief Add a Cnot gate to the output and do some optimize if possible.
 *
 * @param Indexes of the control and target qubits.
 */
void Optimizer::_add_cx(QubitIdType t1, QubitIdType t2, bool do_swap) {
    bool found_match = false;

    if (_qubit_available[t1]) {
        if (!_qubit_available[t2]) {
            for (auto gate : _available_gates[t1] |
                                 std::views::reverse |
                                 std::views::transform([this](size_t id) { return _storage[id]; })) {
                if (gate.get_operation() == CXGate() && gate.get_qubit(0) == t2 && gate.get_qubit(1) == t1) {
                    found_match = true;
                    break;
                }
            }
            if (found_match && do_swap) {
                // NOTE -  -  do the CNOT(t,c)CNOT(c,t) = CNOT(c,t)SWAP(c,t) commutation
                if (std::ranges::any_of(
                        _available_gates[t2] | std::views::transform([this](size_t id) { return _storage[id]; }),
                        [&](QCirGate const& g) { return g == QCirGate{CXGate(), {t2, t1}}; })) {
                    _statistics.DO_SWAP++;
                    spdlog::trace("Apply a do_swap commutation");
                    auto cnot = _store_cx(t1, t2);

                    remove_last_occurrence_if(_gates[t1], [&](size_t id) { return _storage[id] == QCirGate{CXGate(), {t2, t1}}; });
                    remove_last_occurrence_if(_gates[t2], [&](size_t id) { return _storage[id] == QCirGate{CXGate(), {t2, t1}}; });
                    _qubit_available[t1] = false;
                    _qubit_available[t2] = true;
                    _gates[t1].emplace_back(cnot);
                    _gates[t2].emplace_back(cnot);
                    _available_gates[t1].clear();
                    _available_gates[t1].emplace_back(cnot);
                    _available_gates[t2].clear();
                    _available_gates[t2].emplace_back(cnot);
                    std::swap(_permutation.at(t2), _permutation.at(t1));
                    Optimizer::_swap_element(ElementType::h, t1, t2);
                    Optimizer::_swap_element(ElementType::x, t1, t2);
                    Optimizer::_swap_element(ElementType::z, t1, t2);
                    return;
                }
            }
        }
        _available_gates[t1].clear();
        _qubit_available[t1] = false;
    }
    if (!_qubit_available[t2]) {
        _available_gates[t2].clear();
        _qubit_available[t2] = true;
    }
    found_match = false;

    for (auto gate : _available_gates[t1] |
                         std::views::reverse |
                         std::views::transform([this](size_t id) { return _storage[id]; })) {
        if (gate == QCirGate{CXGate(), {t1, t2}}) {
            found_match = true;
            break;
        }
    }
    // NOTE - do CNOT(c,t)CNOT(c,t) = id
    if (found_match) {
        if (std::ranges::any_of(
                _available_gates[t2] | std::views::transform([this](size_t id) { return _storage[id]; }),
                [&](QCirGate const& g) { return g == QCirGate{CXGate(), {t1, t2}}; })) {
            _statistics.CNOT_CANCEL++;
            spdlog::trace("Cancel with previous CX");
            remove_last_occurrence_if(_available_gates[t1], [&](size_t id) { return _storage[id] == QCirGate{CXGate(), {t1, t2}}; });
            remove_last_occurrence_if(_available_gates[t2], [&](size_t id) { return _storage[id] == QCirGate{CXGate(), {t1, t2}}; });
            remove_last_occurrence_if(_gates[t1], [&](size_t id) { return _storage[id] == QCirGate{CXGate(), {t1, t2}}; });
            remove_last_occurrence_if(_gates[t2], [&](size_t id) { return _storage[id] == QCirGate{CXGate(), {t1, t2}}; });
        } else {
            found_match = false;
        }
    }
    if (!found_match) {
        auto cnot = _store_cx(t1, t2);
        _gates[t1].emplace_back(cnot);
        _gates[t2].emplace_back(cnot);
        _available_gates[t1].emplace_back(cnot);
        _available_gates[t2].emplace_back(cnot);
    }
}

bool Optimizer::_replace_cx_and_cz_with_s_and_cx(QubitIdType t1, QubitIdType t2) {
    // NOTE - Checkout t1 as control and t2 as control respectively.
    auto const find_match = [this](QubitIdType t1, QubitIdType t2) -> std::optional<std::pair<QubitIdType, QubitIdType>> {
        for (auto [ctrl, targ] : {std::pair{t1, t2}, std::pair{t2, t1}}) {
            if (std::ranges::none_of(
                    _available_gates[ctrl] | std::views::transform([this](size_t id) { return _storage[id]; }),
                    [&ctrl = ctrl, &targ = targ](QCirGate const& g) {
                        return g == QCirGate{CXGate(), {ctrl, targ}};
                    })) {
                continue;
            }

            if (_qubit_available[targ]) {
                if (std::ranges::any_of(
                        _available_gates[targ] | std::views::transform([this](size_t id) { return _storage[id]; }),
                        [&ctrl = ctrl, &targ = targ](QCirGate const& g) {
                            return g == QCirGate{CXGate(), {ctrl, targ}};
                        })) {
                    return std::make_pair(ctrl, targ);
                }
                continue;
            }
            // NOTE - According to pyzx "There are Z-like gates blocking the CNOT from usage
            //        But if the CNOT can be passed all the way up to these Z-like gates
            //        Then we can commute the CZ gate next to the CNOT and hence use it."
            // NOTE - looking at the gates behind the Z-like gates
            for (auto gate : _gates[targ] | std::views::take(_gates[targ].size() - _available_gates[targ].size()) | std::views::reverse) {
                if (_storage[gate].get_operation() != CXGate())
                    break;
                if (_storage[gate].get_qubit(1) != targ)
                    break;
                // TODO - "_gates[targ][i] == g " might be available too
                if (_storage[gate].get_qubits() == QubitIdList{ctrl, targ}) {
                    return std::make_pair(ctrl, targ);
                }
            }
        }

        return std::nullopt;
    };

    auto const match = find_match(t1, t2);
    if (!match) return false;

    auto [ctrl, targ] = *match;
    auto cnot         = _store_cx(ctrl, targ);

    // NOTE - CNOT-CZ = (S* x id)CNOT (S x S)
    _statistics.CRZ_TRANSFORM++;
    spdlog::trace("Transform CNOT-CZ into (S* x id)CNOT(S x S)");
    if (_qubit_available[targ]) {
        _available_gates[targ].clear();
    }
    remove_last_occurrence_if(_available_gates[ctrl], [this, &ctrl = ctrl, &targ = targ](size_t id) { return _storage[id] == QCirGate{CXGate(), {ctrl, targ}}; });
    remove_last_occurrence_if(_gates[ctrl], [this, &ctrl = ctrl, &targ = targ](size_t id) { return _storage[id] == QCirGate{CXGate(), {ctrl, targ}}; });
    remove_last_occurrence_if(_gates[targ], [this, &ctrl = ctrl, &targ = targ](size_t id) { return _storage[id] == QCirGate{CXGate(), {ctrl, targ}}; });

    auto s1 = _store_sdg(targ);
    auto s2 = _store_s(targ);
    auto s3 = _store_s(ctrl);

    if (!_available_gates[targ].empty()) {
        _gates[targ].insert(dvlab::iterator::prev(_gates[targ].end(), _available_gates[targ].size()), s1);
        _gates[targ].insert(dvlab::iterator::prev(_gates[targ].end(), _available_gates[targ].size()), cnot);
    } else {
        _gates[targ].emplace_back(s1);
        _gates[targ].emplace_back(cnot);
    }
    _gates[targ].emplace_back(s2);
    _available_gates[targ].emplace_back(s2);

    _available_gates[ctrl].emplace_back(cnot);
    _available_gates[ctrl].emplace_back(s3);
    _gates[ctrl].emplace_back(cnot);
    _gates[ctrl].emplace_back(s3);
    return true;
}

/**
 * @brief
 *
 */
void Optimizer::_add_cz(QubitIdType t1, QubitIdType t2, bool do_minimize_czs) {
    if (do_minimize_czs && _replace_cx_and_cz_with_s_and_cx(t1, t2)) {
        return;
    }

    if (_qubit_available[t1]) {
        _available_gates[t1].clear();
        _qubit_available[t1] = false;
    }
    if (_qubit_available[t2]) {
        _available_gates[t2].clear();
        _qubit_available[t2] = false;
    }

    // NOTE - Try to cancel CZ
    for (auto targ_cz : _available_gates[t1] |
                            std::views::filter([&](size_t id) { return _storage[id].get_operation() == CZGate(); })) {
        if (std::ranges::all_of(
                QubitIdList{t1, t2}, [&](auto const& q) {
                    return dvlab::contains(_storage[targ_cz].get_qubits(), q);
                }) &&
            dvlab::contains(_available_gates[t2], targ_cz))  //
        {
            _statistics.CZ_CANCEL++;
            spdlog::trace("Cancel with previous CZ");
            remove_last_occurrence_if(_available_gates[t1], [&](size_t id) { return id == targ_cz; });
            remove_last_occurrence_if(_available_gates[t2], [&](size_t id) { return id == targ_cz; });
            remove_last_occurrence_if(_gates[t1], [&](size_t id) { return id == targ_cz; });
            remove_last_occurrence_if(_gates[t2], [&](size_t id) { return id == targ_cz; });
            return;
        }
    }

    // NOTE - No cancel found

    auto cz = (t1 < t2) ? _store_cz(t1, t2) : _store_cz(t2, t1);
    _gates[t1].emplace_back(cz);
    _gates[t2].emplace_back(cz);
    _available_gates[t1].emplace_back(cz);
    _available_gates[t2].emplace_back(cz);
}

/**
 * @brief Add a single qubit rotate gate to the output.
 *
 * @param target Index of the target qubit
 * @param ph Phase of the gate
 * @param type 0: Z-axis, 1: X-axis, 2: Y-axis
 */
void Optimizer::_add_single_z_rotation_gate(QubitIdType target, dvlab::Phase ph) {
    auto rotate = _store_single_z_rotation_gate(target, ph);
    _gates[target].emplace_back(rotate);
    _available_gates[target].emplace_back(rotate);
}

/**
 * @brief Get the number of two qubit gate, H gate and not pauli gate in a circuit.
 *
 */
std::vector<size_t> Optimizer::_compute_stats(QCir const& circuit) {
    size_t two_qubit = 0, had = 0, non_pauli = 0;
    std::vector<size_t> stats;
    for (auto const& g : circuit.get_gates()) {
        if (g->get_operation() == CXGate() || g->get_operation() == CZGate()) {
            two_qubit++;
        } else if (g->get_operation() == HGate()) {
            had++;
        } else if (g->get_operation() != XGate() && g->get_operation() != ZGate() && g->get_operation() != YGate()) {
            non_pauli++;
        }
    }
    stats.emplace_back(two_qubit);
    stats.emplace_back(had);
    stats.emplace_back(non_pauli);
    return stats;
}

/**
 * @brief Get the swap path from initial permutation to _permutation now
 *
 * @return vector<pair<size_t, size_t>> swap_path
 */
std::vector<std::pair<QubitIdType, QubitIdType>> Optimizer::_get_swap_path() {
    std::vector<std::pair<QubitIdType, QubitIdType>> swap_path;
    std::unordered_map<QubitIdType, QubitIdType> inv_permutation;
    for (auto [i, j] : _permutation) {
        inv_permutation.emplace(j, i);
    }

    for (QubitIdType i = 0; i < _permutation.size(); i++) {
        if (_permutation[i] == i) continue;
        auto q1 = _permutation[i];
        auto q2 = inv_permutation[i];
        swap_path.emplace_back(std::make_pair(i, q2));
        _permutation[q2]    = q1;
        inv_permutation[q1] = q2;
    }
    return swap_path;
}

}  // namespace qsyn::qcir
