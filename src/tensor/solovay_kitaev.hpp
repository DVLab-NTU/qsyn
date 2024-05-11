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
#include <sul/dynamic_bitset.hpp>
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
using BinaryList = std::vector<sul::dynamic_bitset<>>;

namespace tensor {

class SolovayKitaev {
public:
    // clang-format off
    // clang-format-17 formats this line in a weird way
    SolovayKitaev(size_t d, size_t r) : _depth(d), _recursion(r) {};
    // clang-format on

    template <typename U>
    std::optional<QCir> solovay_kitaev_decompose(QTensor<U> const& matrix);

private:
    size_t _depth;
    size_t _recursion;
    BinaryList _binary_list;
    QCir _quantum_circuit;

    template <typename U>
    std::vector<QTensor<U>> _create_gate_list() const;
    template <typename U>
    QTensor<U> _diagonalize(const QTensor<U>& u) const;

    template <typename U>
    QTensor<U> _find_and_insert_closest_u(const std::vector<QTensor<U>>& gate_list, const QTensor<U>& u, std::vector<int>& output_gate) const;

    template <typename U>
    std::pair<QTensor<U>, QTensor<U>> _group_commutator_decompose(const QTensor<U>& u) const;

    template <typename U>
    QTensor<U> _solovay_kitaev_iteration(const std::vector<QTensor<U>>& gate_list, const QTensor<U>& u, size_t n, std::vector<int>& output_gate) const;

    template <typename U>
    std::vector<std::complex<U>> _to_bloch(const QTensor<U>& u) const;

    void _init_binary_list();
    std::vector<int> _adjoint_gate_sequence(std::vector<int> sequence) const;
    void _remove_redundant_gates(std::vector<int>& gate_sequence) const;
    void _save_gates(const std::vector<int>& gate_sequence);
};

/**
 * @brief
 *
 * @tparam U
 * @param matrix
 * @return std::optional<QCir>
 * @reference Dawson, Christopher M., and Michael A. Nielsen. "The solovay-kitaev algorithm." arXiv preprint quant-ph/0505030 (2005).
 * @reference https://github.com/qcc4cp/qcc/blob/main/src/solovay_kitaev.py
 */
template <typename U>
std::optional<QCir> SolovayKitaev::solovay_kitaev_decompose(QTensor<U> const& matrix) {
    assert(matrix.dimension() == 2);

    spdlog::info("Gate list depth: {0}, #Recursions: {1}", _depth, _recursion);

    spdlog::debug("Creating gate list");
    _init_binary_list();
    const std::vector<QTensor<U>> gate_list = _create_gate_list<U>();

    spdlog::debug("Performing SK algorithm");
    std::vector<int> output_gates;
    const U tr_dist = trace_distance(matrix, _solovay_kitaev_iteration(gate_list, matrix, _recursion, output_gates));

    fmt::println("\nTrace distance: {:.{}f}\n", tr_dist, 6);

    _remove_redundant_gates(output_gates);

    _save_gates(output_gates);
    return _quantum_circuit;
}

/**
 * @brief
 *
 * @tparam U
 * @param gate_list usable gates
 * @param u
 * @param recursion number of recursions
 * @param output_gate 1: T, -1: TDG, 0: H
 * @return QTensor<U>
 * @reference Dawson, Christopher M., and Michael A. Nielsen. "The solovay-kitaev algorithm." arXiv preprint quant-ph/0505030 (2005).
 * @reference https://github.com/qcc4cp/qcc/blob/main/src/solovay_kitaev.py
 */
template <typename U>
QTensor<U> SolovayKitaev::_solovay_kitaev_iteration(const std::vector<QTensor<U>>& gate_list, const QTensor<U>& u, size_t recursion, std::vector<int>& output_gate) const {
    if (recursion == 0) {
        return _find_and_insert_closest_u(gate_list, u, output_gate);
    } else {
        std::vector<int> output_gate_u_prev, output_gate_v_prev, output_gate_w_prev;
        const QTensor<U> u_prev = _solovay_kitaev_iteration(gate_list, u, recursion - 1, output_gate_u_prev);
        const QTensor<U> u_mult = tensor_multiply(u, adjoint(u_prev));
        auto const& [v, w]      = _group_commutator_decompose(u_mult);
        const QTensor<U> v_prev = _solovay_kitaev_iteration(gate_list, v, recursion - 1, output_gate_v_prev);
        const QTensor<U> w_prev = _solovay_kitaev_iteration(gate_list, w, recursion - 1, output_gate_w_prev);

        // NOTE - prepare adjointed gate sequence
        const std::vector<int> output_gate_v_prev_adjoint = _adjoint_gate_sequence(output_gate_v_prev);
        const std::vector<int> output_gate_w_prev_adjoint = _adjoint_gate_sequence(output_gate_w_prev);

        output_gate.clear();
        // NOTE - U_n = V_{n-1} W_{n-1} V_{n-1}^\dagger W_{n-1}^\dagger U_{n-1}
        output_gate.insert(output_gate.end(), output_gate_v_prev.begin(), output_gate_v_prev.end());
        output_gate.insert(output_gate.end(), output_gate_w_prev.begin(), output_gate_w_prev.end());
        output_gate.insert(output_gate.end(), output_gate_v_prev_adjoint.begin(), output_gate_v_prev_adjoint.end());
        output_gate.insert(output_gate.end(), output_gate_w_prev_adjoint.begin(), output_gate_w_prev_adjoint.end());
        output_gate.insert(output_gate.end(), output_gate_u_prev.begin(), output_gate_u_prev.end());

        return tensor_multiply(v_prev, tensor_multiply(w_prev, tensor_multiply(adjoint(v_prev), tensor_multiply(adjoint(w_prev), u_prev))));
    }
}

/**
 * @brief Find and insert the closest unitary
 *
 * @tparam U
 * @param gate_list
 * @param u
 * @param output_gate
 * @return QTensor<U>
 */
template <typename U>
QTensor<U> SolovayKitaev::_find_and_insert_closest_u(const std::vector<QTensor<U>>& gate_list, const QTensor<U>& u, std::vector<int>& output_gate) const {
    U min_dist       = 10;
    QTensor<U> min_u = QTensor<U>::identity(1);

    size_t min_index = 0;
    for (size_t i = 0; i < gate_list.size(); i++) {
        const U tr_dist = trace_distance(gate_list[i], u);
        if (min_dist - tr_dist > pow(10, -12)) {
            min_dist  = tr_dist;
            min_u     = gate_list[i];
            min_index = i;
        }
    }
    for (size_t i = 0; i < _binary_list[min_index].size(); i++)
        output_gate.emplace_back(_binary_list[min_index][i]);
    return min_u;
}

/**
 * @brief Perform Group Commutator decomposition
 *
 * @tparam U
 * @param unitary
 * @return std::pair<QTensor<U>, QTensor<U>>
 * @reference Dawson, Christopher M., and Michael A. Nielsen. "The solovay-kitaev algorithm." arXiv preprint quant-ph/0505030 (2005).
 * @reference https://github.com/qcc4cp/qcc/blob/main/src/solovay_kitaev.py
 */
template <typename U>
std::pair<QTensor<U>, QTensor<U>> SolovayKitaev::_group_commutator_decompose(const QTensor<U>& unitary) const {
    assert(unitary.dimension() == 2);
    using namespace std::literals;
    auto const axis = _to_bloch(unitary);
    //  The angle phi comes from eq 10 in 'The Solovay-Kitaev Algorithm' by Dawson, Nielsen.
    const std::complex<U> phi = 2.0 * asin(std::sqrt(std::sqrt(0.5 - 0.5 * cos(axis[3] / 2.0))));
    const QTensor<U> v        = {{cos(phi / 2.0), -1.i * sin(phi / 2.0)}, {-1.i * sin(phi / 2.0), cos(phi / 2.0)}};

    constexpr auto pi = std::numbers::pi_v<U>;
    auto const w      = axis[2].real() > 0
                            ? QTensor<U>{{cos((2 * pi - phi) / 2.0), -1. * sin((2 * pi - phi) / 2.0)},
                                         {sin((2 * pi - phi) / 2.0), cos((2 * pi - phi) / 2.0)}}
                            : QTensor<U>{{cos(phi / 2.0), -1. * sin(phi / 2.0)},
                                         {sin(phi / 2.0), cos(phi / 2.0)}};

    // NOTE - Cannot infer if merging into one line
    const QTensor<U> mult      = tensor_multiply(v, tensor_multiply(w, tensor_multiply(adjoint(v), adjoint(w))));
    const QTensor<U> s         = tensor_multiply(_diagonalize(unitary), adjoint(_diagonalize(mult)));
    const QTensor<U> adjoint_s = adjoint(s);
    // return v_hat w_hat
    return {tensor_multiply(s, tensor_multiply(v, adjoint_s)), tensor_multiply(s, tensor_multiply(w, adjoint_s))};
}

/**
 * @brief Compute angle and axis for a unitary
 *
 * @tparam U
 * @param unitary
 * @return std::vector<std::complex<U>> [nx, ny, nz, angle]
 */
template <typename U>
std::vector<std::complex<U>> SolovayKitaev::_to_bloch(const QTensor<U>& unitary) const {
    assert(unitary.dimension() == 2);
    using namespace std::literals;
    std::vector<std::complex<U>> axis;
    const U angle = (acos((unitary(0, 0) + unitary(1, 1)) / 2.0)).real();
    const U sine  = sin(angle);
    // axis = [nx, ny, nz, angle]
    if (sine < pow(10, -10)) {
        axis = {0, 0, 1, 2 * angle};
    } else {
        axis = {(unitary(0, 1) + unitary(1, 0)) / (sine * 2.i),
                (unitary(0, 1) - unitary(1, 0)) / (sine * 2),
                (unitary(0, 0) - unitary(1, 1)) / (sine * 2.i),
                2 * angle};
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
QTensor<U> SolovayKitaev::_diagonalize(const QTensor<U>& u) const {
    return std::get<1>(u.eigen());
}

/**
 * @brief Create the basic gate list by concatenating Hs and Ts
 *
 * @tparam U
 * @return std::vector<QTensor<U>>
 */
template <typename U>
std::vector<QTensor<U>> SolovayKitaev::_create_gate_list() const {
    std::vector<QTensor<U>> gate_list;
    for (const auto& bits : _binary_list) {
        QTensor<U> u = QTensor<U>::identity(1);
        for (size_t i = 0; i < bits.size(); i++)
            u = (bits[i]) ? tensor_multiply(u, QTensor<U>::pzgate(Phase(1, 4)).to_su2()) : tensor_multiply(u, QTensor<U>::hgate().to_su2());
        gate_list.emplace_back(u);
    }
    return gate_list;
}

}  // namespace tensor

}  // namespace qsyn