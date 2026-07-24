/****************************************************************************
  PackageName  [ synthesis / search ]
  Synopsis     [ LayerGenerator implementations. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./layer_generator.hpp"

#include <numbers>
#include <random>

#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir_gate.hpp"
#include "util/phase.hpp"

namespace qsyn::synthesis::search {

namespace {

dvlab::Phase to_phase(double rad) { return dvlab::Phase{rad, 1e-9}; }

// Identity U3 is a saddle point of the |tr|-based cosine-similarity cost
// (the linear gradient vanishes there because tr((Y⊗...)U_target) collapses
// to zero for unitary targets). To give LBFGS a non-zero gradient at
// iter 0 we seed every new U3 with a small deterministic perturbation
// drawn from a fixed PRNG. Determinism keeps the search reproducible
// across runs.
qcir::Operation u3_random(std::mt19937& rng) {
    std::uniform_real_distribution<double> dist(-0.5, 0.5);  // ~28 degrees
    return qcir::Operation{
        qcir::UGate(to_phase(dist(rng)), to_phase(dist(rng)), to_phase(dist(rng)))};
}

qcir::Operation cx_op() {
    return qcir::Operation{qcir::CXGate()};
}

}  // namespace

qcir::QCir build_root_ansatz(std::size_t n_qubits) {
    qcir::QCir root{n_qubits};
    std::mt19937 rng{0xC0FFEEu};
    for (std::size_t q = 0; q < n_qubits; ++q) {
        root.append(u3_random(rng), {static_cast<qsyn::QubitIdType>(q)});
    }
    return root;
}

std::vector<qcir::QCir> SimpleLayerGenerator::expand(qcir::QCir const& parent) const {
    std::vector<qcir::QCir> children;
    if (_n < 2) return children;

    // Seed each freshly-added U3 with a deterministic-but-distinct PRNG
    // stream so that sibling children have different initial parameters
    // (helps LBFGS escape saddles where multiple equivalent local minima
    // would otherwise collapse onto the same gradient direction).
    std::mt19937 rng{static_cast<std::uint32_t>(0x5EEDu ^ parent.get_num_gates())};

    children.reserve(_n * (_n - 1));
    for (std::size_t c = 0; c < _n; ++c) {
        for (std::size_t t = 0; t < _n; ++t) {
            if (c == t) continue;
            qcir::QCir child = parent;
            auto const cq    = static_cast<qsyn::QubitIdType>(c);
            auto const tq    = static_cast<qsyn::QubitIdType>(t);
            child.append(cx_op(), {cq, tq});
            child.append(u3_random(rng), {cq});
            child.append(u3_random(rng), {tq});
            children.push_back(std::move(child));
        }
    }
    return children;
}

}  // namespace qsyn::synthesis::search
