/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Apply Solovay-Kitaev decomposition to all rotation gates in QCir ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2025 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./solovay_kitaev_all.hpp"

#include <complex>
#include <iostream>
#include <numbers>

#include "cmd/qcir_mgr.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir_gate.hpp"
#include "tensor/qtensor.hpp"
#include "tensor/solovay_kitaev.hpp"
#include "tensor/tensor.hpp"

using qsyn::qcir::QCirMgr;

namespace qsyn::tensor {

namespace {

QTensor<double> get_rx_tensor(double theta) {
    using namespace std::complex_literals;
    return QTensor<double>({{std::complex<double>(std::cos(theta / 2.0), 0), std::complex<double>(0, -std::sin(theta / 2.0))},
                            {std::complex<double>(0, -std::sin(theta / 2.0)), std::complex<double>(std::cos(theta / 2.0), 0)}});
}

QTensor<double> get_ry_tensor(double theta) {
    return QTensor<double>({{std::complex<double>(std::cos(theta / 2.0), 0.0), std::complex<double>(-std::sin(theta / 2.0), 0.0)},
                            {std::complex<double>(std::sin(theta / 2.0), 0.0), std::complex<double>(std::cos(theta / 2.0), 0.0)}});
}

QTensor<double> get_rz_tensor(double theta) {
    using namespace std::complex_literals;
    return QTensor<double>({{std::exp(-1.0i * theta / 2.0), 0.0},
                            {0.0, std::exp(1.0i * theta / 2.0)}});
}

}  // namespace

/**
 * @brief Apply Solovay-Kitaev decomposition to all rotation gates
 *
 * @param mgr Quantum circuit manager
 * @param depth Gate list depth
 * @param recursion Number of recursive refinements
 */
void solovay_kitaev_all(QCirMgr& mgr, size_t depth, size_t recursion) {
    using QCirGate = qsyn::qcir::QCirGate;

    std::vector<QCirGate*> new_gates;

    for (auto* gate : mgr.get()->get_gates()) {
        auto const& op = gate->get_operation();

        QTensor<double> u;
        double theta;
        bool match = false;

        if (op.is<qsyn::qcir::RXGate>()) {
            auto rat = op.get_underlying<qsyn::qcir::RXGate>().get_phase().get_rational();
            theta    = std::numbers::pi * static_cast<double>(rat.numerator()) / static_cast<double>(rat.denominator());
            u        = get_rx_tensor(theta);
            match    = true;
        } else if (op.is<qsyn::qcir::RYGate>()) {
            auto rat = op.get_underlying<qsyn::qcir::RYGate>().get_phase().get_rational();
            theta    = std::numbers::pi * static_cast<double>(rat.numerator()) / static_cast<double>(rat.denominator());
            u        = get_ry_tensor(theta);
            match    = true;
        } else if (op.is<qsyn::qcir::RZGate>()) {
            auto rat = op.get_underlying<qsyn::qcir::RZGate>().get_phase().get_rational();
            theta    = std::numbers::pi * static_cast<double>(rat.numerator()) / static_cast<double>(rat.denominator());
            u        = get_rz_tensor(theta);
            match    = true;
        }

        if (match) {
            size_t qubit = gate->get_qubit(0);
            SolovayKitaev sk(depth, recursion);
            auto opt_circuit = sk.solovay_kitaev_decompose(u);

            if (opt_circuit) {
                for (const auto& g : opt_circuit->get_gates()) {
                    auto shifted_qubits = g->get_qubits();
                    for (auto& q : shifted_qubits) q = qubit;
                    new_gates.emplace_back(new QCirGate(g->get_operation(), shifted_qubits));
                }
            } else {
                spdlog::warn("Failed to decompose gate at qubit {}", qubit);
                new_gates.push_back(gate);
            }
        } else {
            new_gates.push_back(gate);
        }
    }

    mgr.get()->reset();
    size_t max_qubit = 0;
    for (auto* g : new_gates) {
        for (auto q : g->get_qubits())
            max_qubit = std::max(max_qubit, q);
    }

    mgr.get()->add_qubits(max_qubit + 1);
    for (auto* g : new_gates)
        mgr.get()->append(g->get_operation(), g->get_qubits());
}

}  // namespace qsyn::tensor
