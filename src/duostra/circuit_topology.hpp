/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define class Duostra structure ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"

namespace qsyn::duostra {

class CircuitTopology {
public:
    CircuitTopology(std::shared_ptr<qcir::QCir> const& dep);

    std::unique_ptr<CircuitTopology> clone() const;

    void update_available_gates(size_t executed);
    size_t get_num_qubits() const { return _dependency_graph->get_num_qubits(); }
    size_t get_num_gates() const { return _dependency_graph->get_num_gates(); }
    qcir::QCirGate const& get_gate(size_t i) const { return *_dependency_graph->get_gate(i); }
    std::vector<size_t> const& get_available_gates() const { return _available_gates; }

protected:
    std::shared_ptr<qcir::QCir const> _dependency_graph;
    std::vector<size_t> _available_gates;

    bool _is_available(size_t gate_idx) const;

    std::unordered_map<size_t, size_t> _executed_gates;  //  maps gate index to number of gates executed next
};

}  // namespace qsyn::duostra
