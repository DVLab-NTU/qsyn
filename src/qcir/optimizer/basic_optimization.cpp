/****************************************************************************
  FileName     [ basic_optimization.cpp ]
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <functional>
#include <iterator>

#include "../gateType.hpp"
#include "../qcir.hpp"
#include "../qcirGate.hpp"
#include "./optimizer.hpp"
#include "fmt/core.h"
#include "util/logger.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();
extern dvlab_utils::Logger logger;

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
    orig_stats = Optimizer::computeStats(qcir);
    logger.info("Start basic optimization");

    _iter = 0;
    // REVIEW - this is rather a weird logic
    //          I'm only restructuring the code here
    //          consider taking a look at why this is necessary
    QCir result = parseForward(qcir, false, config);
    result = parseBackward(result, false, config);
    result = parseForward(result, false, config);
    prev_stats = Optimizer::computeStats(qcir);
    result = parseBackward(result, true, config);
    stats = Optimizer::computeStats(result);
    result = parseForward(result, true, config);

    while (!stop_requested() && _iter < config.maxIter &&
           (prev_stats[0] > stats[0] || prev_stats[1] > stats[1] || prev_stats[2] > stats[2])) {
        prev_stats = stats;

        result = parseBackward(result, true, config);
        stats = Optimizer::computeStats(result);
        // TODO - Find a more efficient way
        result = parseForward(result, true, config);
    }

    if (stop_requested()) {
        logger.warning("optimization interrupted");
        return std::nullopt;
    }

    logger.info("Basic optimization finished after {} iterations.", _iter * 2 + 1)
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
QCir Optimizer::parseOnce(QCir const& qcir, bool reversed, bool minimizeCZ, BasicOptimizationConfig const& config) {
    (reversed)
        ? logger.debug("Start parsing backward")
        : logger.debug("Start parsing forward");

    reset(qcir);
    qcir.updateTopoOrder();
    std::vector<QCirGate*> gates = qcir.getTopoOrderedGates();
    if (reversed) {
        std::reverse(gates.begin(), gates.end());
    }
    for (auto& gate : gates) {
        parseGate(gate, config.doSwap, minimizeCZ);
    }

    // TODO - Find a method to avoid using parameter "erase"
    for (auto& t : _hadamards) {
        addHadamard(t, false);
    }

    for (auto& t : _zs) {
        addRotateGate(t, Phase(1), GateType::P);
    }

    QCir result = fromStorage(qcir.getNQubit(), reversed);
    result.setFileName(qcir.getFileName());
    result.addProcedures(qcir.getProcedures());

    for (auto& t : _xs) {
        QCirGate* notGate = new XGate(_gateCnt);
        notGate->addQubit(t, true);
        _gateCnt++;
        Optimizer::_addGate2Circuit(result, notGate, reversed);
    }

    _swaps = Optimizer::get_swap_path();

    for (auto& [c, t] : _swaps) {
        // TODO - Use SWAP gate to replace cnots
        QCirGate* cnot_1 = new CXGate(_gateCnt);
        cnot_1->addQubit(c, false);
        cnot_1->addQubit(t, true);
        QCirGate* cnot_2 = new CXGate(_gateCnt + 1);
        cnot_2->addQubit(t, false);
        cnot_2->addQubit(c, true);
        QCirGate* cnot_3 = new CXGate(_gateCnt + 2);
        cnot_3->addQubit(c, false);
        cnot_3->addQubit(t, true);
        _gateCnt += 3;
        Optimizer::_addGate2Circuit(result, cnot_1, reversed);
        Optimizer::_addGate2Circuit(result, cnot_2, reversed);
        Optimizer::_addGate2Circuit(result, cnot_3, reversed);
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
    for (auto& line : split(statistics_str, "\n")) {
        logger.debug("{}", line);
    }
    logger.debug("");

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
bool Optimizer::parseGate(QCirGate* gate, bool doSwap, bool minimizeCZ) {
    permuteGate(gate);

    if (gate->getType() == GateType::H) {
        matchHadamard(gate);
        return true;
    }

    if (gate->getType() == GateType::X) {
        matchX(gate);
        return true;
    }

    if (isSingleRotateZ(gate)) {
        matchRotateZ(gate);
        return true;
    }

    if (gate->getType() != GateType::CX && gate->getType() != GateType::CZ) {
        return false;
    }

    if (gate->getType() == GateType::CZ) {
        matchCZ(gate, doSwap, minimizeCZ);
    }

    if (gate->getType() == GateType::CX) {
        matchCX(gate, doSwap, minimizeCZ);
    }

    return false;
}

/**
 * @brief Add the gate from storage (_gates) to the circuit
 *
 * @param circuit
 */
QCir Optimizer::fromStorage(size_t nQubits, bool reversed) {
    QCir circuit;
    circuit.addQubit(nQubits);

    while (any_of(_gates.begin(), _gates.end(), [](auto& p_g) { return p_g.second.size(); })) {
        ordered_hashset<size_t> available_id;
        for (auto& [q, gs] : _gates) {
            while (gs.size()) {
                QCirGate* g = gs[0];
                if (g->getType() != GateType::CX && g->getType() != GateType::CZ) {
                    Optimizer::_addGate2Circuit(circuit, g, reversed);
                    gs.erase(gs.begin());
                } else if (available_id.contains(g->getId())) {
                    available_id.erase(g->getId());
                    size_t q2 = (q == g->getControl()._qubit) ? g->getTarget()._qubit : g->getControl()._qubit;
                    _gates[q2].erase(--(find_if(_gates[q2].rbegin(), _gates[q2].rend(), [&](QCirGate* _g) { return g->getId() == _g->getId(); })).base());
                    Optimizer::_addGate2Circuit(circuit, g, reversed);
                    gs.erase(gs.begin());
                } else {
                    bool type = !(g->getType() == GateType::CZ || g->getControl()._qubit == q);
                    std::vector<size_t> removed;
                    available_id.emplace(g->getId());
                    for (size_t i = 1; i < gs.size(); i++) {
                        QCirGate* g2 = gs[i];
                        if ((!type && isSingleRotateZ(g2)) || (type && isSingleRotateX(g2))) {
                            Optimizer::_addGate2Circuit(circuit, g2, reversed);
                            removed.emplace(removed.begin(), i);
                        } else if (g2->getType() != GateType::CX && g2->getType() != GateType::CZ) {
                            break;
                        } else if ((!type && (g2->getType() == GateType::CZ || g2->getControl()._qubit == q)) ||
                                   (type && (g2->getType() == GateType::CX && g2->getTarget()._qubit == q))) {
                            if (available_id.contains(g2->getId())) {
                                available_id.erase(g2->getId());
                                size_t q2 = q == g2->getControl()._qubit ? g2->getTarget()._qubit : g2->getControl()._qubit;
                                _gates[q2].erase(--(find_if(_gates[q2].rbegin(), _gates[q2].rend(), [&](QCirGate* _g) { return g2->getId() == _g->getId(); })).base());
                                Optimizer::_addGate2Circuit(circuit, g2, reversed);
                                removed.emplace(removed.begin(), i);
                            } else {
                                available_id.emplace(g2->getId());
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
void Optimizer::addHadamard(size_t target, bool erase) {
    QCirGate* had = new HGate(_gateCnt);
    had->addQubit(target, true);
    _gateCnt++;
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
void Optimizer::addCX(size_t ctrl, size_t targ, bool doSwap) {
    bool found_match = false;

    if (_availty[ctrl] == true) {
        if (_availty[targ] == false) {
            for (int i = _available[ctrl].size() - 1; i >= 0; i--) {
                QCirGate* g = _available[ctrl][i];
                if (g->getType() == GateType::CX && g->getControl()._qubit == targ && g->getTarget()._qubit == ctrl) {
                    found_match = true;
                    break;
                }
            }
            if (found_match && doSwap) {
                // NOTE -  -  do the CNOT(t,c)CNOT(c,t) = CNOT(c,t)SWAP(c,t) commutation
                if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl); })) {
                    _statistics.DO_SWAP++;
                    logger.trace("Apply a do_swap commutation");
                    QCirGate* cnot = new CXGate(_gateCnt);
                    cnot->addQubit(ctrl, false);
                    cnot->addQubit(targ, true);
                    _gateCnt++;
                    _gates[ctrl].erase(--(find_if(_gates[ctrl].rbegin(), _gates[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl); })).base());
                    _gates[targ].erase(--(find_if(_gates[targ].rbegin(), _gates[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, targ, ctrl); })).base());
                    _availty[ctrl] = false;
                    _availty[targ] = true;
                    _gates[ctrl].emplace_back(cnot);
                    _gates[targ].emplace_back(cnot);
                    _available[ctrl].clear();
                    _available[ctrl].emplace_back(cnot);
                    _available[targ].clear();
                    _available[targ].emplace_back(cnot);
                    std::swap(_permutation.at(targ), _permutation.at(ctrl));
                    Optimizer::swapElement(0, ctrl, targ);
                    Optimizer::swapElement(1, ctrl, targ);
                    Optimizer::swapElement(2, ctrl, targ);
                    return;
                }
            }
        }
        _available[ctrl].clear();
        _availty[ctrl] = false;
    }
    if (_availty[targ] == false) {
        _available[targ].clear();
        _availty[targ] = true;
    }
    found_match = false;

    for (int i = _available[ctrl].size() - 1; i >= 0; i--) {
        QCirGate* g = _available[ctrl][i];
        if (g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ) {
            found_match = true;
            break;
        }
    }
    // NOTE - do CNOT(c,t)CNOT(c,t) = id
    if (found_match) {
        if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })) {
            _statistics.CNOT_CANCEL++;
            logger.trace("Cancel with previous CX");
            _available[ctrl].erase(--(find_if(_available[ctrl].rbegin(), _available[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
            _available[targ].erase(--(find_if(_available[targ].rbegin(), _available[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
            _gates[ctrl].erase(--(find_if(_gates[ctrl].rbegin(), _gates[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
            _gates[targ].erase(--(find_if(_gates[targ].rbegin(), _gates[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
        } else {
            found_match = false;
        }
    }
    if (!found_match) {
        QCirGate* cnot = new CXGate(_gateCnt);
        cnot->addQubit(ctrl, false);
        cnot->addQubit(targ, true);
        _gateCnt++;
        _gates[ctrl].emplace_back(cnot);
        _gates[targ].emplace_back(cnot);
        _available[ctrl].emplace_back(cnot);
        _available[targ].emplace_back(cnot);
    }
}

bool Optimizer::replace_CX_and_CZ_with_S_and_CX(size_t t1, size_t t2) {
    bool found_match = false;
    // NOTE - Checkout t1 as control and t2 as control respectively.
    size_t ctrl, targ;
    QCirGate* cnot;
    for (size_t i = 0; i < 2; i++) {
        ctrl = !i ? t1 : t2;
        targ = !i ? t2 : t1;
        for (auto& g : _available[ctrl]) {
            if (g->getType() == GateType::CX && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ) {
                cnot = new CXGate(_gateCnt);
                _gateCnt++;
                cnot->addQubit(g->getControl()._qubit, false);
                cnot->addQubit(g->getTarget()._qubit, true);
                if (_availty[targ] == true) {
                    if (count_if(_available[targ].begin(), _available[targ].end(), [&](QCirGate* _g) { return Optimizer::TwoQubitGateExist(_g, GateType::CX, ctrl, targ); })) {
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
                    if (_gates[targ][i]->getType() != GateType::CX || _gates[targ][i]->getTarget()._qubit != targ)
                        break;
                    // TODO - "_gates[targ][i] == g " might be available too
                    if (_gates[targ][i]->getType() == GateType::CX && _gates[targ][i]->getControl()._qubit == ctrl && _gates[targ][i]->getTarget()._qubit == targ) {
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
    logger.trace("Transform CNOT-CZ into (S* x id)CNOT(S x S)");
    if (_availty[targ] == true) {
        // REVIEW -  pyzx/optimize/line.339 has a bug
        //  _availty[targ] = 1;
        _available[targ].clear();
    }
    _available[ctrl].erase(--(find_if(_available[ctrl].rbegin(), _available[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
    _gates[ctrl].erase(--(find_if(_gates[ctrl].rbegin(), _gates[ctrl].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());
    _gates[targ].erase(--(find_if(_gates[targ].rbegin(), _gates[targ].rend(), [&](QCirGate* g) { return Optimizer::TwoQubitGateExist(g, GateType::CX, ctrl, targ); })).base());

    QCirGate* s1 = new SDGGate(_gateCnt);
    s1->addQubit(targ, true);
    _gateCnt++;
    QCirGate* s2 = new SGate(_gateCnt);
    s2->addQubit(targ, true);
    _gateCnt++;
    QCirGate* s3 = new SGate(_gateCnt);
    s3->addQubit(ctrl, true);
    _gateCnt++;

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
void Optimizer::addCZ(size_t t1, size_t t2, bool minimizeCZ) {
    size_t ctrl = -1, targ = -1;
    bool found_match = false;
    QCirGate* targ_cz = nullptr;

    if (minimizeCZ && replace_CX_and_CZ_with_S_and_CX(t1, t2)) {
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
        if ((g->getType() == GateType::CZ && g->getControl()._qubit == t1 && g->getTarget()._qubit == t2) ||
            (g->getType() == GateType::CZ && g->getControl()._qubit == t2 && g->getTarget()._qubit == t1)) {
            found_match = true;
            targ_cz = g;
            break;
        }
    }

    if (found_match) {
        if (count_if(_available[t2].begin(), _available[t2].end(), [&](QCirGate* g) { return g == targ_cz; })) {
            _statistics.CZ_CANCEL++;
            logger.trace("Cancel with previous CZ");
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
        QCirGate* cz = new CZGate(_gateCnt);
        if (t1 < t2) {
            cz->addQubit(t1, false);
            cz->addQubit(t2, true);
        } else {
            cz->addQubit(t2, false);
            cz->addQubit(t1, true);
        }

        _gateCnt++;
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
bool Optimizer::TwoQubitGateExist(QCirGate* g, GateType gt, size_t ctrl, size_t targ) {
    return (g->getType() == gt && g->getControl()._qubit == ctrl && g->getTarget()._qubit == targ);
}

/**
 * @brief Add a single qubit rotate gate to the output.
 *
 * @param target Index of the target qubit
 * @param ph Phase of the gate
 * @param type 0: Z-axis, 1: X-axis, 2: Y-axis
 */
void Optimizer::addRotateGate(size_t target, Phase ph, GateType const& type) {
    QCirGate* rotate = nullptr;
    if (type == GateType::P) {
        rotate = new PGate(_gateCnt);
    } else if (type == GateType::PX) {
        rotate = new PXGate(_gateCnt);
    } else if (type == GateType::PY) {
        rotate = new PYGate(_gateCnt);
    } else {
        logger.fatal("wrong type!! Type shoud be P, PX or PY");
        return;
    }
    rotate->setRotatePhase(ph);
    rotate->addQubit(target, true);
    _gates[target].emplace_back(rotate);
    _available[target].emplace_back(rotate);
    _gateCnt++;
}

/**
 * @brief Get the number of two qubit gate, H gate and not pauli gate in a circuit.
 *
 */
std::vector<size_t> Optimizer::computeStats(QCir const& circuit) {
    size_t two_qubit = 0, had = 0, non_pauli = 0;
    std::vector<size_t> stats;
    for (const auto& g : circuit.updateTopoOrder()) {
        GateType type = g->getType();
        if (type == GateType::CX || type == GateType::CZ) {
            two_qubit++;
        } else if (type == GateType::H) {
            had++;
        } else if (type != GateType::X && type != GateType::Y && type != GateType::Z && g->getPhase() != Phase(1)) {
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
std::vector<std::pair<size_t, size_t>> Optimizer::get_swap_path() {
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
