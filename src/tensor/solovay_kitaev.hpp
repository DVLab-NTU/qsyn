/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor Solovay-Kitaev algorithm structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <tl/to.hpp>
#include <vector>

#include "qcir/qcir.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tensor/qtensor.hpp"
#include "tensor/tensor.hpp"

namespace qsyn {

using qcir::QCir;
using tensor::QTensor;
using tensor::Tensor;
using BinaryList = std::vector<std::vector<bool>>;

namespace tensor {

class SolovayKitaev {
public:
    SolovayKitaev(size_t d, size_t r) : _depth(d), _recursion(r){};

    template <typename U>
    std::optional<QCir> solovay_kitaev_decompose(QTensor<U> const& matrix);

private:
    size_t _depth;
    size_t _recursion;

    QCir _quantum_circuit;
    template <typename U>
    std::vector<QTensor<U>> _create_gate_list();
    template <typename U>
    QTensor<U> _diagonalize(QTensor<U> u);

    template <typename U>
    QTensor<U> _find_and_insert_closest_u(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, std::vector<int>& output_gate);

    template <typename U>
    std::vector<QTensor<U>> _gc_decompose(QTensor<U> u);

    template <typename U>
    QTensor<U> _solovay_kitaev_iteration(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, size_t n, std::vector<int>& output_gate);

    template <typename U>
    std::vector<std::complex<double>> _to_bloch(QTensor<U> u);

    BinaryList _init_binary_list() const;
    void _dagger_matrices(std::vector<int>& sequence);
    // void _remove_redundant_gates(std::vector<int>& gate_sequence);
    void _save_gates(const std::vector<int>& gate_sequence);
};

/**
 * @brief Perform Solovay-Kitaev decomposition
 *
 * @tparam U
 * @param matrix
 * @reference Dawson, Christopher M., and Michael A. Nielsen. "The solovay-kitaev algorithm." arXiv preprint quant-ph/0505030 (2005).
 * @reference https://github.com/qcc4cp/qcc/blob/main/src/solovay_kitaev.py
 */
template <typename U>
std::optional<QCir> SolovayKitaev::solovay_kitaev_decompose(QTensor<U> const& matrix) {
    assert(matrix.dimension() == 2);

    spdlog::info("Gate list depth: {0}, #Recursions: {1}", _depth, _recursion);

    spdlog::debug("Creating gate list");
    const std::vector<QTensor<U>> gate_list = _create_gate_list<U>();

    spdlog::debug("Performing SK algorithm");
    std::vector<int> output_gates;

    BinaryList bin_list = _init_binary_list();

    const double tr_dist = trace_distance(matrix, _solovay_kitaev_iteration(gate_list, bin_list, matrix, _recursion, output_gates));

    fmt::println("\nTrace distance: {:.{}f}\n", tr_dist, 6);

    // _remove_redundant_gates(output_gates);

    _save_gates(output_gates);
    return _quantum_circuit;
}

/**
 * @brief
 *
 * @tparam U
 * @param gate_list usable gates
 * @param bin_list
 * @param u
 * @param recursion number of recursions
 * @param output_gate 1: T, -1: TDG, 0: H
 * @return QTensor<U>
 * @reference Dawson, Christopher M., and Michael A. Nielsen. "The solovay-kitaev algorithm." arXiv preprint quant-ph/0505030 (2005).
 * @reference https://github.com/qcc4cp/qcc/blob/main/src/solovay_kitaev.py
 */
template <typename U>
QTensor<U> SolovayKitaev::_solovay_kitaev_iteration(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, size_t recursion, std::vector<int>& output_gate) {
    if (recursion == 0) {
        return _find_and_insert_closest_u(gate_list, bin_list, u, output_gate);
    } else {
        std::vector<int> output_gate_u_prev;
        const QTensor<U> u_prev   = _solovay_kitaev_iteration(gate_list, bin_list, u, recursion - 1, output_gate_u_prev);
        QTensor<U> u_prev_adjoint = u_prev;
        u_prev_adjoint.adjoint();
        const QTensor<U> u_mult    = tensor_multiply(u, u_prev_adjoint);
        std::vector<QTensor<U>> vw = _gc_decompose(u_mult);
        const QTensor<U> v         = vw[0];
        const QTensor<U> w         = vw[1];
        std::vector<int> output_gate_v_prev;
        std::vector<int> output_gate_w_prev;
        const QTensor<U> v_prev = _solovay_kitaev_iteration(gate_list, bin_list, v, recursion - 1, output_gate_v_prev);
        const QTensor<U> w_prev = _solovay_kitaev_iteration(gate_list, bin_list, w, recursion - 1, output_gate_w_prev);

        QTensor<U> v_prev_adjoint = v_prev;
        QTensor<U> w_prev_adjoint = w_prev;
        v_prev_adjoint.adjoint();
        w_prev_adjoint.adjoint();

        std::vector<int> output_gate_v_prev_adjoint = output_gate_v_prev;
        std::vector<int> output_gate_w_prev_adjoint = output_gate_w_prev;
        _dagger_matrices(output_gate_v_prev_adjoint);
        _dagger_matrices(output_gate_w_prev_adjoint);

        output_gate.clear();
        // NOTE - U_n = V_{n-1} W_{n-1} V_{n-1}^\dagger W_{n-1}^\dagger U_{n-1}
        output_gate.insert(output_gate.end(), output_gate_v_prev.begin(), output_gate_v_prev.end());
        output_gate.insert(output_gate.end(), output_gate_w_prev.begin(), output_gate_w_prev.end());
        output_gate.insert(output_gate.end(), output_gate_v_prev_adjoint.begin(), output_gate_v_prev_adjoint.end());
        output_gate.insert(output_gate.end(), output_gate_w_prev_adjoint.begin(), output_gate_w_prev_adjoint.end());
        output_gate.insert(output_gate.end(), output_gate_u_prev.begin(), output_gate_u_prev.end());

        return tensor_multiply(v_prev, tensor_multiply(w_prev, tensor_multiply(v_prev_adjoint, tensor_multiply(w_prev_adjoint, u_prev))));
    }
}

/**
 * @brief Find and insert the closest unitary
 *
 * @tparam U
 * @param gate_list
 * @param bin_list
 * @param u
 * @param output_gate
 * @return QTensor<U>
 */
template <typename U>
QTensor<U> SolovayKitaev::_find_and_insert_closest_u(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, std::vector<int>& output_gate) {
    double min_dist  = 10;
    QTensor<U> min_u = QTensor<U>::identity(1);

    size_t min_index = 0;
    for (size_t i = 0; i < gate_list.size(); i++) {
        const double tr_dist = trace_distance(gate_list[i], u);
        if (min_dist - tr_dist > pow(10, -12)) {
            min_dist  = tr_dist;
            min_u     = gate_list[i];
            min_index = i;
        }
    }
    for (const auto& bit : bin_list[min_index])
        output_gate.emplace_back(bit);

    return min_u;
}

/**
 * @brief Perform GC decomposition
 *
 * @tparam U
 * @param unitary
 * @return std::vector<QTensor<U>>
 * @reference https://github.com/qcc4cp/qcc/blob/main/src/solovay_kitaev.py
 */
template <typename U>
std::vector<QTensor<U>> SolovayKitaev::_gc_decompose(QTensor<U> unitary) {
    assert(unitary.dimension() == 2);
    using namespace std::literals;
    std::vector<std::complex<double>> axis = _to_bloch(unitary);
    // The angle phi calculation
    const std::complex<double> phi = 2.0 * asin(std::sqrt(std::sqrt(0.5 - 0.5 * cos(axis[3] / 2.0))));
    const QTensor<U> v             = {{cos(phi / 2.0), -1.i * sin(phi / 2.0)}, {-1.i * sin(phi / 2.0), cos(phi / 2.0)}};
    QTensor<U> w                   = QTensor<U>::identity(1);

    constexpr auto pi = std::numbers::pi_v<double>;
    if (axis[2].real() > 0) {
        w = {{cos((2 * pi - phi) / 2.0), -1. * sin((2 * pi - phi) / 2.0)},
             {sin((2 * pi - phi) / 2.0), cos((2 * pi - phi) / 2.0)}};
    } else {
        w = {{cos(phi / 2.0), -1. * sin(phi / 2.0)},
             {sin(phi / 2.0), cos(phi / 2.0)}};
    }
    // QTensor<U> ud        = (diagonalize(unitary));
    QTensor<U> adjoint_v = v;
    QTensor<U> adjoint_w = w;
    adjoint_v.adjoint();
    adjoint_w.adjoint();
    // NOTE - Cannot infer if merging into one line
    const QTensor<U> mult = tensor_multiply(v, tensor_multiply(w, tensor_multiply(adjoint_v, adjoint_w)));
    QTensor<U> vwvdwd     = _diagonalize(mult);
    vwvdwd.adjoint();

    const QTensor<U> s = tensor_multiply(_diagonalize(unitary), vwvdwd);

    QTensor<U> adjoint_s = s;
    adjoint_s.adjoint();

    // return v_hat w_hat
    return {tensor_multiply(s, tensor_multiply(v, adjoint_s)), tensor_multiply(s, tensor_multiply(w, adjoint_s))};
}

/**
 * @brief To bloch sphere
 *
 * @tparam U
 * @param unitary
 * @return std::vector<std::complex<double>> [nx, ny, nz, angle]
 */
template <typename U>
std::vector<std::complex<double>> SolovayKitaev::_to_bloch(QTensor<U> unitary) {
    assert(unitary.dimension() == 2);
    using namespace std::literals;
    std::vector<std::complex<double>> axis;
    const double angle = (acos((unitary(0, 0) + unitary(1, 1)) / 2.0)).real();
    const double sine  = sin(angle);
    if (sine < pow(10, -10)) {
        // nx, ny, nz, angle
        axis.emplace_back(0);
        axis.emplace_back(0);
        axis.emplace_back(1);
        axis.emplace_back(2 * angle);

    } else {
        // nx, ny, nz, angle
        axis.emplace_back((unitary(0, 1) + unitary(1, 0)) / (sine * 2.i));
        axis.emplace_back((unitary(0, 1) - unitary(1, 0)) / (sine * 2));
        axis.emplace_back((unitary(0, 0) - unitary(1, 1)) / (sine * 2.i));
        axis.emplace_back(2 * angle);
    }
    return axis;
}

/**
 * @brief Diagonalize the unitary
 *
 * @tparam U
 * @param u
 * @return QTensor<U>
 */
template <typename U>
QTensor<U> SolovayKitaev::_diagonalize(QTensor<U> u) {
    return std::get<1>(u.eigen());
}

template <typename U>
std::vector<QTensor<U>> SolovayKitaev::_create_gate_list() {
    std::vector<QTensor<U>> base = {QTensor<U>::hgate().to_su2(),
                                    QTensor<U>::pzgate(Phase(1, 4)).to_su2()};
    std::vector<QTensor<U>> gate_list;
    const BinaryList bin_list = _init_binary_list();

    for (const auto& bits : bin_list) {
        QTensor<U> u = QTensor<U>::identity(1);
        for (const auto& bit : bits)
            u = (bit) ? tensor_multiply(u, base[1]) : tensor_multiply(u, base[0]);
        gate_list.emplace_back(u);
    }
    return gate_list;
}

}  // namespace tensor

}  // namespace qsyn
