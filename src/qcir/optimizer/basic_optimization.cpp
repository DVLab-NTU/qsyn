/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <functional>
#include <iterator>

#include "../gate_type.hpp"
#include "../qcir.hpp"
#include "../qcir_gate.hpp"
#include "./optimizer.hpp"
#include "fmt/core.h"
#include "util/logger.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();
extern dvlab::Logger LOGGER;

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
    LOGGER.info("Start basic optimization");

    _iter = 0;
    // REVIEW - this is rather a weird logic
    //          I'm only restructuring the code here
    //          consider taking a look at why this is necessary
    QCir result = parse_forward(qcir, false, config);
    result = parse_backward(result, false, config);
    result = parse_forward(result, false, config);
    prev_stats = Optimizer::_compute_stats(qcir);
    result = parse_backward(result, true, config);
    stats = Optimizer::_compute_stats(result);
    result = parse_forward(result, true, config);

    while (!stop_requested() && _iter < config.maxIter &&
           (prev_stats[0] > stats[0] || prev_stats[1] > stats[1] || prev_stats[2] > stats[2])) {
        prev_stats = stats;

        result = parse_backward(result, true, config);
        stats = Optimizer::_compute_stats(result);
        // TODO - Find a more efficient way
        result = parse_forward(result, true, config);
    }

    if (stop_requested()) {
        LOGGER.warning("optimization interrupted");
        return std::nullopt;
    }

    LOGGER.info("Basic optimization finished after {} iterations.", _iter * 2 + 1)
        .indent()
        .info("Two-qubit gates: {} → {}", orig_stats[0], stats[0])
        .info("Hadamard gates : {} → {}", orig_stats[1], stats[1])
        .info("Non-Pauli gates: {} → {}", orig_stats[2], stats[2])
        .unindent();

    return result;
}

/**
 * @brief Parse through the gates according to topological order and optimize the circuit
 *
 * @return QCir* : new Circuit
 */
QCir Optimizer::_parse_once(QCir const& qcir, bool reversed, bool do_minimize_czs, BasicOptimizationConfig const& config) {
    (reversed)
        ? LOGGER.debug("Start parsing backward")
        : LOGGER.debug("Start parsing forward");

    reset(qcir);
    qcir.update_topological_order();
    std::vector<QCirGate*> gates = qcir.get_topologically_ordered_gates();
    if (reversed) {
        std::reverse(gates.begin(), gates.end());
    }
    for (auto& gate : gates) {
        parse_gate(gate, config.doSwap, do_minimize_czs);
    }

    // TODO - Find a method to avoid using parameter "erase"
    for (auto& t : _hadamards) {
        _add_hadamard(t, false);
    }

    for (auto& t : _zs) {
        _add_rotation_gate(t, Phase(1), GateType::p);
    }

    QCir result = _build_from_storage(qcir.get_num_qubits(), reversed);
    result.set_filename(qcir.get_filename());
    result.add_procedures(qcir.get_procedures());

    for (auto& t : _xs) {
        QCirGate* not_gate = new XGate(_gate_count);
        not_gate->add_qubit(t, true);
        _gate_count++;
        Optimizer::_add_gate_to_circuit(result, not_gate, reversed);
    }

    _swaps = Optimizer::_get_swap_path();

    for (auto& [c, t] : _swaps) {
        // TODO - Use SWAP gate to replace cnots
        QCirGate* cnot_1 = new CXGate(_gate_count);
        cnot_1->add_qubit(c, false);
        cnot_1->add_qubit(t, true);
        QCirGate* cnot_2 = new CXGate(_gate_count + 1);
        cnot_2->add_qubit(t, false);
        cnot_2->add_qubit(c, true);
        QCirGate* cnot_3 = new CXGate(_gate_count + 2);
        cnot_3->add_qubit(c, false);
        cnot_3->add_qubit(t, true);
        _gate_count += 3;
        Optimizer::_add_gate_to_circuit(result, cnot_1, reversed);
        Optimizer::_add_gate_to_circuit(result, cnot_2, reversed);
        Optimizer::_add_gate_to_circuit(result, cnot_3, reversed);
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
    fmt::format_to(std::back_inserter(statistics_str), "    Crz transform  : {}\n", _statistics.CRZ_TRACSFORM);
    fmt::format_to(std::back_inserter(statistics_str), "    Do swap        : {}\n", _statistics.DO_SWAP);
    fmt::format_to(std::back_inserter(statistics_str), "  Note: {} CZs had been transformed into CXs.\n", _statistics.CZ2CX);
    fmt::format_to(std::back_inserter(statistics_str), "        {} CXs had been transformed into CZs.\n", _statistics.CX2CZ);
    fmt::format_to(std::back_inserter(statistics_str), "  Note: {} swap gates had been added in the swap path.\n", _swaps.size());

    if (config.printStatistics) {
        fmt::println("{}", statistics_str);
    }
    for (auto& line : dvlab::str::split(statistics_str, "\n")) {
        LOGGER.debug("{}", line);
    }
    LOGGER.debug("");

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
bool Optimizer::parse_gate(QCirGate* gate, bool do_swap, bool minimize_czs) {
    _permute_gates(gate);

    if (gate->get_type() == GateType::h) {
        _match_hadamards(gate);
        return true;
    }

    if (gate->get_type() == GateType::x) {
        _match_xs(gate);
        return true;
    }

    if (is_single_z_rotation(gate)) {
        _match_z_rotations(gate);
        return true;
    }

    if (gate->get_type() != GateType::cx && gate->get_type() != GateType::cz) {
        return false;
    }

    if (gate->get_type() == GateType::cz) {
        _match_czs(gate, do_swap, minimize_czs);
    }

    if (gate->get_type() == GateType::cx) {
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
    QCir circuit;
    circuit.add_qubits(n_qubits);

    while (any_of(_gates.begin(), _gates.end(), [](auto& p_g) { return p_g.second.size(); })) {
        ordered_hashset<size_t> available_id;
        for (auto& [q, gs] : _gates) {
            while (gs.size()) {
                QCirGate* g = gs[0];
                if (g->get_type() != GateType::cx && g->get_type() != GateType::cz) {
                    Optimizer::_add_gate_to_circuit(circuit, g, reversed);
                    gs.erase(gs.begin());
                } else if (available_id.contains(g->get_id())) {
                    available_id.erase(g->get_id());
                    size_t q2 = (q == g->get_control()._qubit) ? g->get_targets()._qubit : g->get_control()._qubit;
                    _gates[q2].erase(--(find_if(_gates[q2].rbegin(), _gates[q2].rend(), [&](QCirGate* gate_other) { return g->get_id() == gate_other->get_id(); })).base());
                    Optimizer::_add_gate_to_circuit(circuit, g, reversed);
                    gs.erase(gs.begin());
                } else {
                    bool type = !(g->get_type() == GateType::cz || g->get_control()._qubit == q);
                    std::vector<size_t> removed;
                    available_id.emplace(g->get_id());
                    for (size_t i = 1; i < gs.size(); i++) {
                        QCirGate* g2 = gs[i];
                        if ((!type && is_single_z_rotation(g2)) || (type && is_single_x_rotation(g2))) {
                            Optimizer::_add_gate_to_circuit(circuit, g2, reversed);
                            removed.emplace(removed.begin(), i);
                        } else if (g2->get_type() != GateType::cx && g2->get_type() != GateType::cz) {
                            break;
                        } else if ((!type && (g2->get_type() == GateType::cz || g2->get_control()._qubit == q)) ||
                                   (type && (g2->get_type() == GateType::cx && g2->get_targets()._qubit == q))) {
                            if (available_id.contains(g2->get_id())) {
                                available_id.erase(g2->get_id());
                                size_t q2 = q == g2->get_control()._qubit ? g2->get_targets()._qubit : g2->get_control()._qubit;
                                _gates[q2].erase(--(find_if(_gates[q2].rbegin(), _gates[q2].rend(), [&](QCirGate* gate_other) { return g2->get_id() == gate_other->get_id(); })).base());
                                Optimizer::_add_gate_to_circuit(circuit, g2, reversed);
                                removed.emplace(removed.begin(), i);
                            } else {
                                available_id.emplace(g2->get_id());
                            }
                        } else {
                            break;
                        }
                    }
                    for (size_t i : removed) {
                        gs.erase(gs.begin() + i);
                    }
                    break;
                }
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
void Optimizer::_add_hadamard(size_t target, bool erase) {
    QCirGate* had = new HGate(_gate_count);
    had->add_qubit(target, true);
    _gate_count++;
    _gates[target].emplace_back(had);
    if (erase) _hadamards.erase(target);
    _available[target].clear();
    _availty[target] = false;
}

/**
 * @brief Add a Cnot gate to the output and do some optimize if possible.
 *
 * @param Indexes of the control and target qubits.
 */
void Optimizer::_add_cx(size_t t1, size_t t2, bool do_swap) {
    bool found_match = false;

    if (_availty[t1] == true) {
        if (_availty[t2] == false) {
            for (int i = _available[t1].size() - 1; i >= 0; i--) {
                QCirGate* g = _available[t1][i];
                if (g->get_type() == GateType::cx && g->get_control()._qubit == t2 && g->get_targets()._qubit == t1) {
                    found_match = true;
                    break;
                }
            }
            if (found_match && do_swap) {
                // NOTE -  -  do the CNOT(t,c)CNOT(c,t) = CNOT(c,t)SWAP(c,t) commutation
                if (count_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t2, t1); })) {
                    _statistics.DO_SWAP++;
                    LOGGER.trace("Apply a do_swap commutation");
                    QCirGate* cnot = new CXGate(_gate_count);
                    cnot->add_qubit(t1, false);
                    cnot->add_qubit(t2, true);
                    _gate_count++;
                    _gates[t1].erase(--(find_if(_gates[t1].rbegin(), _gates[t1].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t2, t1); })).base());
                    _gates[t2].erase(--(find_if(_gates[t2].rbegin(), _gates[t2].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t2, t1); })).base());
                    _availty[t1] = false;
                    _availty[t2] = true;
                    _gates[t1].emplace_back(cnot);
                    _gates[t2].emplace_back(cnot);
                    _available[t1].clear();
                    _available[t1].emplace_back(cnot);
                    _available[t2].clear();
                    _available[t2].emplace_back(cnot);
                    std::swap(_permutation.at(t2), _permutation.at(t1));
                    Optimizer::_swap_element(0, t1, t2);
                    Optimizer::_swap_element(1, t1, t2);
                    Optimizer::_swap_element(2, t1, t2);
                    return;
                }
            }
        }
        _available[t1].clear();
        _availty[t1] = false;
    }
    if (_availty[t2] == false) {
        _available[t2].clear();
        _availty[t2] = true;
    }
    found_match = false;

    for (int i = _available[t1].size() - 1; i >= 0; i--) {
        QCirGate* g = _available[t1][i];
        if (g->get_type() == GateType::cx && g->get_control()._qubit == t1 && g->get_targets()._qubit == t2) {
            found_match = true;
            break;
        }
    }
    // NOTE - do CNOT(c,t)CNOT(c,t) = id
    if (found_match) {
        if (count_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t1, t2); })) {
            _statistics.CNOT_CANCEL++;
            LOGGER.trace("Cancel with previous CX");
            _available[t1].erase(--(find_if(_available[t1].rbegin(), _available[t1].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t1, t2); })).base());
            _available[t2].erase(--(find_if(_available[t2].rbegin(), _available[t2].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t1, t2); })).base());
            _gates[t1].erase(--(find_if(_gates[t1].rbegin(), _gates[t1].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t1, t2); })).base());
            _gates[t2].erase(--(find_if(_gates[t2].rbegin(), _gates[t2].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, t1, t2); })).base());
        } else {
            found_match = false;
        }
    }
    if (!found_match) {
        QCirGate* cnot = new CXGate(_gate_count);
        cnot->add_qubit(t1, false);
        cnot->add_qubit(t2, true);
        _gate_count++;
        _gates[t1].emplace_back(cnot);
        _gates[t2].emplace_back(cnot);
        _available[t1].emplace_back(cnot);
        _available[t2].emplace_back(cnot);
    }
}

bool Optimizer::_replace_cx_and_cz_with_s_and_cx(size_t t1, size_t t2) {
    bool found_match = false;
    // NOTE - Checkout t1 as control and t2 as control respectively.
    size_t ctrl, targ;
    QCirGate* cnot;
    for (size_t i = 0; i < 2; i++) {
        ctrl = !i ? t1 : t2;
        targ = !i ? t2 : t1;
        for (auto& g : _available[ctrl]) {
            if (g->get_type() == GateType::cx && g->get_control()._qubit == ctrl && g->get_targets()._qubit == targ) {
                cnot = new CXGate(_gate_count);
                _gate_count++;
                cnot->add_qubit(g->get_control()._qubit, false);
                cnot->add_qubit(g->get_targets()._qubit, true);
                if (_availty[targ] == true) {
                    if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* gate_other) { return Optimizer::two_qubit_gate_exists(gate_other, GateType::cx, ctrl, targ); })) {
                        found_match = true;
                        break;
                    } else
                        continue;
                }
                // NOTE - According to pyzx "There are Z-like gates blocking the CNOT from usage
                //        But if the CNOT can be passed all the way up to these Z-like gates
                //        Then we can commute the CZ gate next to the CNOT and hence use it."
                // NOTE - looking at the gates behind the Z-like gates
                for (int i = _gates[targ].size() - _available[targ].size() - 1; i >= 0; i--) {
                    if (_gates[targ][i]->get_type() != GateType::cx || _gates[targ][i]->get_targets()._qubit != targ)
                        break;
                    // TODO - "_gates[targ][i] == g " might be available too
                    if (_gates[targ][i]->get_type() == GateType::cx && _gates[targ][i]->get_control()._qubit == ctrl && _gates[targ][i]->get_targets()._qubit == targ) {
                        found_match = true;
                        break;
                    }
                }
                if (found_match) break;
            }
        }
        if (found_match) break;
    }

    if (!found_match) return false;
    // NOTE - CNOT-CZ = (S* x id)CNOT (S x S)
    _statistics.CRZ_TRACSFORM++;
    LOGGER.trace("Transform CNOT-CZ into (S* x id)CNOT(S x S)");
    if (_availty[targ] == true) {
        // REVIEW -  pyzx/optimize/line.339 has a bug
        //  _availty[targ] = 1;
        _available[targ].clear();
    }
    _available[ctrl].erase(--(find_if(_available[ctrl].rbegin(), _available[ctrl].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, ctrl, targ); })).base());
    _gates[ctrl].erase(--(find_if(_gates[ctrl].rbegin(), _gates[ctrl].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, ctrl, targ); })).base());
    _gates[targ].erase(--(find_if(_gates[targ].rbegin(), _gates[targ].rend(), [&](QCirGate* g) { return Optimizer::two_qubit_gate_exists(g, GateType::cx, ctrl, targ); })).base());

    QCirGate* s1 = new SDGGate(_gate_count);
    s1->add_qubit(targ, true);
    _gate_count++;
    QCirGate* s2 = new SGate(_gate_count);
    s2->add_qubit(targ, true);
    _gate_count++;
    QCirGate* s3 = new SGate(_gate_count);
    s3->add_qubit(ctrl, true);
    _gate_count++;

    if (_available[targ].size()) {
        _gates[targ].insert(_gates[targ].end() - _available[targ].size(), s1);
        _gates[targ].insert(_gates[targ].end() - _available[targ].size(), cnot);
    } else {
        _gates[targ].emplace_back(s1);
        _gates[targ].emplace_back(cnot);
    }
    _gates[targ].emplace_back(s2);
    _available[targ].emplace_back(s2);

    _available[ctrl].emplace_back(cnot);
    _available[ctrl].emplace_back(s3);
    _gates[ctrl].emplace_back(cnot);
    _gates[ctrl].emplace_back(s3);
    return true;
}

/**
 * @brief
 *
 */
void Optimizer::_add_cz(size_t t1, size_t t2, bool do_minimize_czs) {
    size_t ctrl = -1, targ = -1;
    bool found_match = false;
    QCirGate* targ_cz = nullptr;

    if (do_minimize_czs && _replace_cx_and_cz_with_s_and_cx(t1, t2)) {
        return;
    }

    if (_availty[t1] == true) {
        _available[t1].clear();
        _availty[t1] = false;
    }
    if (_availty[t2] == true) {
        _available[t2].clear();
        _availty[t2] = false;
    }

    // NOTE - Try to cancel CZ
    for (auto& g : _available[t1]) {
        if ((g->get_type() == GateType::cz && g->get_control()._qubit == t1 && g->get_targets()._qubit == t2) ||
            (g->get_type() == GateType::cz && g->get_control()._qubit == t2 && g->get_targets()._qubit == t1)) {
            found_match = true;
            targ_cz = g;
            break;
        }
    }

    if (found_match) {
        if (count_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g) { return g == targ_cz; })) {
            _statistics.CZ_CANCEL++;
            LOGGER.trace("Cancel with previous CZ");
            _available[t1].erase(--(find_if(_available[t1].rbegin(), _available[t1].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
            _available[t2].erase(--(find_if(_available[t2].rbegin(), _available[t2].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
            _gates[t1].erase(--(find_if(_gates[t1].rbegin(), _gates[t1].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
            _gates[t2].erase(--(find_if(_gates[t2].rbegin(), _gates[t2].rend(), [&](QCirGate* g) { return g == targ_cz; })).base());
        } else {
            found_match = false;
        }
    }
    // NOTE - No cancel found
    if (!found_match) {
        QCirGate* cz = new CZGate(_gate_count);
        if (t1 < t2) {
            cz->add_qubit(t1, false);
            cz->add_qubit(t2, true);
        } else {
            cz->add_qubit(t2, false);
            cz->add_qubit(t1, true);
        }

        _gate_count++;
        _gates[t1].emplace_back(cz);
        _gates[t2].emplace_back(cz);
        _available[t1].emplace_back(cz);
        _available[t2].emplace_back(cz);
    }
}

/**
 * @brief Predicate function called by addCX and addCZ.
 *
 */
bool Optimizer::two_qubit_gate_exists(QCirGate* g, GateType gt, size_t ctrl, size_t targ) {
    return (g->get_type() == gt && g->get_control()._qubit == ctrl && g->get_targets()._qubit == targ);
}

/**
 * @brief Add a single qubit rotate gate to the output.
 *
 * @param target Index of the target qubit
 * @param ph Phase of the gate
 * @param type 0: Z-axis, 1: X-axis, 2: Y-axis
 */
void Optimizer::_add_rotation_gate(size_t target, Phase ph, GateType const& type) {
    QCirGate* rotate = nullptr;
    if (type == GateType::p) {
        rotate = new PGate(_gate_count);
    } else if (type == GateType::px) {
        rotate = new PXGate(_gate_count);
    } else if (type == GateType::py) {
        rotate = new PYGate(_gate_count);
    } else {
        LOGGER.fatal("wrong type!! Type shoud be P, PX or PY");
        return;
    }
    rotate->set_phase(ph);
    rotate->add_qubit(target, true);
    _gates[target].emplace_back(rotate);
    _available[target].emplace_back(rotate);
    _gate_count++;
}

/**
 * @brief Get the number of two qubit gate, H gate and not pauli gate in a circuit.
 *
 */
std::vector<size_t> Optimizer::_compute_stats(QCir const& circuit) {
    size_t two_qubit = 0, had = 0, non_pauli = 0;
    std::vector<size_t> stats;
    for (auto const& g : circuit.update_topological_order()) {
        GateType type = g->get_type();
        if (type == GateType::cx || type == GateType::cz) {
            two_qubit++;
        } else if (type == GateType::h) {
            had++;
        } else if (type != GateType::x && type != GateType::y && type != GateType::z && g->get_phase() != Phase(1)) {
            non_pauli++;
        }
    }
    stats.emplace_back(two_qubit);
    stats.emplace_back(had);
    stats.emplace_back(non_pauli);
    return stats;
}

/**
 * @brief Get the swap path from intial permutaion to _permutation now
 *
 * @return vector<pair<size_t, size_t>> swap_path
 */
std::vector<std::pair<size_t, size_t>> Optimizer::_get_swap_path() {
    std::vector<std::pair<size_t, size_t>> swap_path;
    std::unordered_map<size_t, size_t> inv_permutation;
    for (auto [i, j] : _permutation) {
        inv_permutation.emplace(j, i);
    }

    for (size_t i = 0; i < _permutation.size(); i++) {
        if (_permutation[i] == i) continue;
        size_t q1 = _permutation[i], q2 = inv_permutation[i];
        swap_path.emplace_back(std::make_pair(i, q2));
        _permutation[q2] = q1;
        inv_permutation[q1] = q2;
    }
    return swap_path;
}
