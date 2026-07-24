/****************************************************************************
  PackageName  [ synthesis / search ]
  Synopsis     [ LayerGenerator interface: given a parent partial circuit
                 of n qubits, emit a (small, finite) set of children that
                 each extend the parent by one "layer". The default
                 SimpleLayerGenerator emits one child per (control,
                 target) ordered CX pair, decorated with U3s on both
                 affected qubits; this is BQSKit's textbook generator.

                 More elaborate generators (e.g. CouplingLayerGenerator
                 that respects a coupling map, or DiscreteLayerGenerator
                 from QFAST) plug in by deriving from this base.
                 Connectivity is OUT OF SCOPE for this PR -- we always
                 assume full all-to-all coupling. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include "qcir/qcir.hpp"

namespace qsyn::synthesis::search {

class LayerGenerator {
public:
    virtual ~LayerGenerator() = default;
    // Return a list of new partial circuits, each = `parent` + one new
    // layer. Output circuits have *fresh* UGate parameters (zeros) on
    // every freshly added U3 -- the caller is expected to instantiate.
    [[nodiscard]] virtual std::vector<qcir::QCir> expand(qcir::QCir const& parent) const = 0;
};

// Building block: CX(ctrl, targ) sandwiched by U3 on both ctrl and targ.
// One child per ordered (ctrl, targ) pair with ctrl != targ.
class SimpleLayerGenerator final : public LayerGenerator {
public:
    explicit SimpleLayerGenerator(std::size_t n_qubits) : _n(n_qubits) {}
    [[nodiscard]] std::vector<qcir::QCir> expand(qcir::QCir const& parent) const override;

private:
    std::size_t _n;
};

// Seed root for the search: an n-qubit circuit with one U3 per qubit
// (the "free" single-qubit fan-out before any CX). All U3 parameters
// start at zero.
[[nodiscard]] qcir::QCir build_root_ansatz(std::size_t n_qubits);

}  // namespace qsyn::synthesis::search
