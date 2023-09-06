/****************************************************************************
  PackageName  [ optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <set>
#include <unordered_map>

#include "util/ordered_hashset.hpp"

class QCir;
class QCirGate;
class Phase;
enum class GateType;

using Qubit2Gates = std::unordered_map<size_t, std::vector<QCirGate*>>;

class Optimizer {
public:
    Optimizer() {}

    void reset(QCir const& qcir);

    // Predicate function && Utils
    bool two_qubit_gate_exists(QCirGate* g, GateType gt, size_t ctrl, size_t targ);
    bool is_single_z_rotation(QCirGate*);
    bool is_single_x_rotation(QCirGate*);
    bool is_double_qubit_gate(QCirGate*);
    QCirGate* get_available_z_rotation(size_t t);

    // basic optimization
    struct BasicOptimizationConfig {
        bool doSwap;
        bool separateCorrection;
        size_t maxIter;
        bool printStatistics;
    };
    std::optional<QCir> basic_optimization(QCir const& qcir, BasicOptimizationConfig const& config);
    QCir parse_forward(QCir const& qcir, bool do_minimize_czs, BasicOptimizationConfig const& config);
    QCir parse_backward(QCir const& qcir, bool do_minimize_czs, BasicOptimizationConfig const& config);
    bool parse_gate(QCirGate*, bool do_swap, bool do_minimize_czs);

    // trivial optimization
    std::optional<QCir> trivial_optimization(QCir const& qcir);

private:
    size_t _iter = 0;
    Qubit2Gates _gates;
    Qubit2Gates _available;
    std::vector<bool> _availty;

    std::unordered_map<size_t, size_t> _permutation;
    ordered_hashset<size_t> _hadamards;
    ordered_hashset<size_t> _xs;
    ordered_hashset<size_t> _zs;
    std::vector<std::pair<size_t, size_t>> _swaps;

    size_t _gate_count = 0;

    struct Statistics {
        size_t FUSE_PHASE = 0;
        size_t X_CANCEL = 0;
        size_t CNOT_CANCEL = 0;
        size_t CZ_CANCEL = 0;
        size_t HS_EXCHANGE = 0;
        size_t CRZ_TRACSFORM = 0;
        size_t DO_SWAP = 0;
        size_t CZ2CX = 0;
        size_t CX2CZ = 0;
    } _statistics;

    // Utils
    void _toggle_element(GateType const& type, size_t element);
    void _swap_element(size_t type, size_t e1, size_t e2);
    static std::vector<size_t> _compute_stats(QCir const& circuit);

    QCir _parse_once(QCir const& qcir, bool reversed, bool do_minimize_czs, BasicOptimizationConfig const& config);

    // basic optimization subroutines

    void _permute_gates(QCirGate* gate);

    void _match_hadamards(QCirGate* gate);
    void _match_xs(QCirGate* gate);
    void _match_z_rotations(QCirGate* gate);
    void _match_czs(QCirGate* gate, bool do_swap, bool do_minimize_czs);
    void _match_cxs(QCirGate* gate, bool do_swap, bool do_minimize_czs);

    void _add_hadamard(size_t, bool erase);
    bool _replace_cx_and_cz_with_s_and_cx(size_t t1, size_t t2);
    void _add_cz(size_t t1, size_t t2, bool do_minimize_czs);
    void _add_cx(size_t t1, size_t t2, bool do_swap);
    void _add_rotation_gate(size_t target, Phase ph, GateType const& type);

    QCir _build_from_storage(size_t n_qubits, bool reversed);

    std::vector<std::pair<size_t, size_t>> _get_swap_path();
    void _add_gate_to_circuit(QCir& circuit, QCirGate* gate, bool prepend);

    // trivial optimization subroutines

    std::vector<QCirGate*> _get_first_layer_gates(QCir& qcir, bool from_last = false);
    void _cancel_double_gate(QCir& qcir, QCirGate* prev_gate, QCirGate* gate);
    void _fuse_z_phase(QCir& qcir, QCirGate* prev_gate, QCirGate* gate);
};