/****************************************************************************
  PackageName  [ qcir/optimizer ]
  Synopsis     [ Define class Optimizer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <unordered_map>

#include "qcir/qcir_gate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/ordered_hashset.hpp"

namespace dvlab {

class Phase;

}

namespace qsyn::qcir {

class QCir;
enum class GateRotationCategory;
using Qubit2Gates = std::unordered_map<QubitIdType, std::vector<QCirGate*>>;

class Optimizer {
public:
    Optimizer() {}

    void reset(QCir const& qcir);

    // Predicate function && Utils
    bool two_qubit_gate_exists(QCirGate* g, GateRotationCategory gt, QubitIdType ctrl, QubitIdType targ);
    bool is_single_z_rotation(QCirGate* g);
    bool is_single_x_rotation(QCirGate* g);
    bool is_double_qubit_gate(QCirGate* g);
    QCirGate* get_available_z_rotation(QubitIdType t);

    // basic optimization
    struct BasicOptimizationConfig {
        bool doSwap             = true;
        bool separateCorrection = false;
        size_t maxIter          = 1000;
        bool printStatistics    = false;
    };
    std::optional<QCir> basic_optimization(QCir const& qcir, BasicOptimizationConfig const& config);
    QCir parse_forward(QCir const& qcir, bool do_minimize_czs, BasicOptimizationConfig const& config);
    QCir parse_backward(QCir const& qcir, bool do_minimize_czs, BasicOptimizationConfig const& config);
    bool parse_gate(QCirGate* gate, bool do_swap, bool do_minimize_czs);

    // trivial optimization
    std::optional<QCir> trivial_optimization(QCir const& qcir);

private:
    size_t _iter = 0;
    std::vector<std::unique_ptr<QCirGate>> _storage;
    Qubit2Gates _gates;
    Qubit2Gates _available;
    std::vector<bool> _availty;

    std::unordered_map<QubitIdType, QubitIdType> _permutation;
    dvlab::utils::ordered_hashset<QubitIdType> _hadamards;
    dvlab::utils::ordered_hashset<QubitIdType> _xs;
    dvlab::utils::ordered_hashset<QubitIdType> _zs;
    std::vector<std::pair<QubitIdType, QubitIdType>> _swaps;

    struct Statistics {
        size_t FUSE_PHASE    = 0;
        size_t X_CANCEL      = 0;
        size_t CNOT_CANCEL   = 0;
        size_t CZ_CANCEL     = 0;
        size_t HS_EXCHANGE   = 0;
        size_t CRZ_TRACSFORM = 0;
        size_t DO_SWAP       = 0;
        size_t CZ2CX         = 0;
        size_t CX2CZ         = 0;
    } _statistics;

    // Utils
    enum class ElementType {
        h,
        x,
        z
    };
    void _toggle_element(ElementType type, QubitIdType element);
    void _swap_element(ElementType type, QubitIdType e1, QubitIdType e2);
    static std::vector<size_t> _compute_stats(QCir const& circuit);

    QCir _parse_once(QCir const& qcir, bool reversed, bool do_minimize_czs, BasicOptimizationConfig const& config);

    // basic optimization subroutines

    void _permute_gates(QCirGate* gate);

    void _match_hadamards(QCirGate* gate);
    void _match_xs(QCirGate* gate);
    void _match_z_rotations(QCirGate* gate);
    void _match_czs(QCirGate* gate, bool do_swap, bool do_minimize_czs);
    void _match_cxs(QCirGate* gate, bool do_swap, bool do_minimize_czs);

    void _add_hadamard(QubitIdType target, bool erase);
    bool _replace_cx_and_cz_with_s_and_cx(QubitIdType t1, QubitIdType t2);
    void _add_cz(QubitIdType t1, QubitIdType t2, bool do_minimize_czs);
    void _add_cx(QubitIdType t1, QubitIdType t2, bool do_swap);
    void _add_rotation_gate(QubitIdType target, dvlab::Phase ph, GateRotationCategory const& rotation_category);

    QCir _build_from_storage(size_t n_qubits, bool reversed);

    std::vector<std::pair<QubitIdType, QubitIdType>> _get_swap_path();

    // trivial optimization subroutines

    std::vector<QCirGate*> _get_first_layer_gates(QCir& qcir, bool from_last = false);
    void _cancel_double_gate(QCir& qcir, QCirGate* prev_gate, QCirGate* gate);
    void _fuse_z_phase(QCir& qcir, QCirGate* prev_gate, QCirGate* gate);
    void _fuse_x_phase(QCir& qcir, QCirGate* prev_gate, QCirGate* gate);
    void _partial_zx_optimization(QCir& qcir);

    inline QCirGate* _store_x(QubitIdType qubit) {
        _storage.emplace_back(std::make_unique<QCirGate>(_storage.size(), GateRotationCategory::px, QubitIdList{qubit}, dvlab::Phase(1)));
        return _storage.back().get();
    }

    inline QCirGate* _store_h(QubitIdType qubit) {
        _storage.emplace_back(std::make_unique<QCirGate>(_storage.size(), GateRotationCategory::h, QubitIdList{qubit}, dvlab::Phase(1)));
        return _storage.back().get();
    }

    inline QCirGate* _store_s(QubitIdType qubit) {
        _storage.emplace_back(std::make_unique<QCirGate>(_storage.size(), GateRotationCategory::pz, QubitIdList{qubit}, dvlab::Phase(1, 2)));
        return _storage.back().get();
    }

    inline QCirGate* _store_sdg(QubitIdType qubit) {
        _storage.emplace_back(std::make_unique<QCirGate>(_storage.size(), GateRotationCategory::pz, QubitIdList{qubit}, dvlab::Phase(-1, 2)));
        return _storage.back().get();
    }

    inline QCirGate* _store_cx(QubitIdType ctrl, QubitIdType targ) {
        _storage.emplace_back(std::make_unique<QCirGate>(_storage.size(), GateRotationCategory::px, QubitIdList{ctrl, targ}, dvlab::Phase(1)));
        return _storage.back().get();
    }

    inline QCirGate* _store_cz(QubitIdType ctrl, QubitIdType targ) {
        _storage.emplace_back(std::make_unique<QCirGate>(_storage.size(), GateRotationCategory::pz, QubitIdList{ctrl, targ}, dvlab::Phase(1)));
        return _storage.back().get();
    }

    inline QCirGate* _store_rotation_gate(QubitIdType target, dvlab::Phase ph, GateRotationCategory const& rotation_category) {
        _storage.emplace_back(std::make_unique<QCirGate>(_storage.size(), rotation_category, QubitIdList{target}, ph));
        return _storage.back().get();
    }
};

}  // namespace qsyn::qcir
