/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor decomposer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <tl/to.hpp>

#include "qcir/qcir.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tensor/qtensor.hpp"
#include "tensor/tensor.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

namespace qsyn {

using qcir::QCir;
using tensor::QTensor;
using tensor::Tensor;
using BinaryList = std::vector<std::vector<bool>>;

namespace tensor {
class SolovayKitaev {
private:
    size_t _depth;
    size_t _recursion;
    BinaryList _init_binary_list() const;
    void _remove_redundant_gates(std::vector<bool>& gate_sequence);

public:
    SolovayKitaev(size_t d, size_t r) : _depth(d), _recursion(r){};
    template <typename U>
    std::vector<std::complex<double>> u_to_bloch(QTensor<U> u);
    // template <typename U>
    // QTensor<U> to_su2(QTensor<U> const & u);

    template <typename U>
    std::vector<QTensor<U>> create_gate_list();
    template <typename U>
    QTensor<U> diagonalize(QTensor<U> u);

    template <typename U>
    QTensor<U> find_closest_u(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, std::vector<bool>& output_gate);

    template <typename U>
    std::vector<QTensor<U>> gc_decomp(QTensor<U> u);

    template <typename U>
    QTensor<U> solovay_kitaev_loop(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, size_t n, std::vector<bool>& output_gate);

    template <typename U>
    void solovay_kitaev_decompose(QTensor<U> const& matrix);
};

BinaryList SolovayKitaev::_init_binary_list() const {
    BinaryList bin_list;
    for (size_t i = 1; i <= _depth; i++) {
        for (size_t j = 0; j < gsl::narrow<size_t>(std::pow(2, i)); j++) {
            boost::dynamic_bitset<> bitset(i, j);
            std::vector<bool> bit_vector;
            for (size_t k = 0; k < i; ++k)
                bit_vector.emplace_back(bitset[k]);
            bin_list.emplace_back(bit_vector);
        }
    }
    return bin_list;
}

void SolovayKitaev::_remove_redundant_gates(std::vector<bool>& gate_sequence) {
    size_t counter = 0;
    while (counter < gate_sequence.size() - 1) {
        if (!gate_sequence[counter] && !gate_sequence[counter + 1]) {
            gate_sequence.erase(gate_sequence.begin() + gsl::narrow<int>(counter), gate_sequence.begin() + gsl::narrow<int>(counter) + 2);
            if (counter < 8)
                counter = 0;
            else
                counter -= 8;
        } else if (gate_sequence[counter] && gate_sequence[counter + 1] && gate_sequence[counter + 2] && gate_sequence[counter + 3] && gate_sequence[counter + 4] && gate_sequence[counter + 5] && gate_sequence[counter + 6] && gate_sequence[counter + 7]) {
            gate_sequence.erase(gate_sequence.begin() + gsl::narrow<int>(counter), gate_sequence.begin() + gsl::narrow<int>(counter) + 8);
        } else
            counter++;
    }
}

template <typename U>
std::vector<QTensor<U>> SolovayKitaev::create_gate_list() {
    std::vector<QTensor<U>> base = {QTensor<U>::hgate().to_su2(),
                                   QTensor<U>::pxgate(Phase(1 / 4)).to_su2()};
    std::vector<QTensor<U>> gate_list;
    // REVIEW - may need refactors
    BinaryList bin_list = _init_binary_list();

    for (const auto& bits : bin_list) {
        QTensor<U> u = QTensor<U>::identity(1);
        for (const auto& bit : bits)
            u = (bit) ? tensor_multiply(u, base[1]) : tensor_multiply(u, base[0]);
        gate_list.emplace_back(u);
    }
    return gate_list;
}

template <typename U>
void SolovayKitaev::solovay_kitaev_decompose(QTensor<U> const& matrix) {
    assert(matrix.dimension() == 2);
    // TODO - Move your code here, you may also create new files src/tensor/

    spdlog::info("Gate list depth: {}", _depth);
    spdlog::info("#Recursions: {}", _recursion);

    spdlog::info("Creating gate list");
    std::vector<QTensor<U>> gate_list = create_gate_list<U>();

    spdlog::info("Performing SK algorithm");
    std::vector<bool> output_gate;
    BinaryList bin_list = _init_binary_list();

    const double tr_dist = trace_distance(matrix, solovay_kitaev_loop(gate_list, bin_list, matrix, _recursion, output_gate));
    fmt::println("Trace distance: {}", tr_dist);

    _remove_redundant_gates(output_gate);
}

template <typename U>
QTensor<U> SolovayKitaev::solovay_kitaev_loop(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, size_t recursion, std::vector<bool>& output_gate) {
    if (recursion == 0) {
        QTensor<U> re = find_closest_u(gate_list, bin_list, u, output_gate);
        return re;
    } else {
        QTensor<U> u_p         = solovay_kitaev_loop(gate_list, bin_list, u, recursion - 1, output_gate);
        QTensor<U> u_p_adjoint = u_p;
        u_p_adjoint.adjoint();
        QTensor<U> u_mult = tensor_multiply(u, u_p_adjoint);
        std::vector<QTensor<U>> vw = gc_decomp(u_mult);
        QTensor<U> v               = vw[0];
        QTensor<U> w               = vw[1];
        QTensor<U> v_p             = solovay_kitaev_loop(gate_list, bin_list, v, recursion - 1, output_gate);
        QTensor<U> w_p             = solovay_kitaev_loop(gate_list, bin_list, w, recursion - 1, output_gate);

        QTensor<U> v_p_adjoint = v_p;
        QTensor<U> w_p_adjoint = w_p;
        v_p_adjoint.adjoint();
        w_p_adjoint.adjoint();

        QTensor<U> re = tensor_multiply(v_p, tensor_multiply(w_p, tensor_multiply(v_p_adjoint, tensor_multiply(w_p_adjoint, u_p))));
        return re;
    }
}

template <typename U>
QTensor<U> SolovayKitaev::find_closest_u(const std::vector<QTensor<U>>& gate_list, BinaryList& bin_list, QTensor<U> u, std::vector<bool>& output_gate) {
    double min_dist  = 10;
    QTensor<U> min_u  = QTensor<U>::identity(1);

    size_t min_index = 0;
    for (size_t i = 0; i < gate_list.size(); i++) {
        QTensor<U> temp = gate_list[i];
        double tr_dist = (trace_distance(temp, u));
        if (min_dist - tr_dist > pow(10, -12)) {
            min_dist  = tr_dist;
            min_u     = temp;
            min_index = i;
        }
    }
    output_gate.insert(output_gate.end(), bin_list[min_index].begin(), bin_list[min_index].end());
    return min_u;
}

template <typename U>
std::vector<QTensor<U>> SolovayKitaev::gc_decomp(QTensor<U> unitary) {
    assert(unitary.dimension() == 2);
    std::vector<std::complex<double>> axis = u_to_bloch(unitary);
    std::complex<double> result(0, 1);
    std::complex<double> neg(-1, 0);
    // The angle phi calculation
    std::complex<double> phi = 2.0 * asin(std::sqrt(std::sqrt(0.5 - 0.5 * cos(axis[3] / 2.0))));
    QTensor<U> v              = {{cos(phi / 2.0), neg * result * sin(phi / 2.0)}, {neg * result * sin(phi / 2.0), cos(phi / 2.0)}};
    QTensor<U> w              = QTensor<U>::identity(1);

    // FIXME - Replace M_PI
    if (axis[2].real() > 0) {
        w = {{cos((2 * M_PI - phi) / 2.0), neg * sin((2 * M_PI - phi) / 2.0)},
             {sin((2 * M_PI - phi) / 2.0), cos((2 * M_PI - phi) / 2.0)}};
        // w = {{cos((2 * M_PI - phi) / 2.0), sin( (2 * M_PI - phi) / 2.0)} , {neg*sin( (2 * M_PI - phi) / 2.0), cos( (2 * M_PI - phi) / 2.0)}};
    } else {
        w = {{cos(phi / 2.0), neg * sin(phi / 2.0)},
             {sin(phi / 2.0), cos(phi / 2.0)}};
        // w = {{cos(phi / 2.0), sin(phi / 2.0)} , {neg*sin(phi / 2.0), cos(phi / 2.0)}};
    }
    QTensor<U> ud        = (diagonalize(unitary));
    QTensor<U> adjoint_v = v;
    QTensor<U> adjoint_w = w;
    adjoint_v.adjoint();
    adjoint_w.adjoint();
    QTensor<U> mult = tensor_multiply(v, tensor_multiply(w, tensor_multiply(adjoint_v, adjoint_w)));
    QTensor<U> vwvdwd = diagonalize(mult);
    vwvdwd.adjoint();

    QTensor<U> s = tensor_multiply(ud, vwvdwd);

    QTensor<U> adjoint_s = s;
    adjoint_s.adjoint();

    QTensor<U> v_hat           = tensor_multiply(s, tensor_multiply(v, adjoint_s));
    QTensor<U> w_hat           = tensor_multiply(s, tensor_multiply(w, adjoint_s));
    std::vector<QTensor<U>> vw = {v_hat, w_hat};

    return vw;
}

template <typename U>
std::vector<std::complex<double>> SolovayKitaev::u_to_bloch(QTensor<U> unitary) {
    assert(unitary.dimension() == 2);
    std::vector<std::complex<double>> axis;
    auto matrix        = unitary;
    const double angle = (acos((matrix(0, 0) + matrix(1, 1)) / 2.0)).real();
    const double sine  = sin(angle);
    if (sine < pow(10, -10)) {
        std::complex<double> nx(0, 0);
        std::complex<double> ny(0, 0);
        std::complex<double> nz(1, 0);
        std::complex<double> angle_2(2 * angle, 0);

        axis.push_back(nx);
        axis.push_back(ny);
        axis.push_back(nz);
        axis.push_back(angle_2);

    } else {
        std::complex<double> j_2(0, 2);
        std::complex<double> nx = (matrix(0, 1) + matrix(1, 0)) / (sine * j_2);
        std::complex<double> ny = (matrix(0, 1) - matrix(1, 0)) / (sine * 2);
        std::complex<double> nz = (matrix(0, 0) - matrix(1, 1)) / (sine * j_2);
        std::complex<double> angle_2(2 * angle, 0);

        axis.push_back(nx);
        axis.push_back(ny);
        axis.push_back(nz);
        axis.push_back(angle_2);
    }
    return axis;
}

template <typename U>
QTensor<U> SolovayKitaev::diagonalize(QTensor<U> u) {
    std::tuple b = u.eigen();
    QTensor<U> value  = std::get<0>(b);
    QTensor<U> vector = std::get<1>(b);
    return std::get<1>(u.eigen());
    /*
    auto eigenvectors = vector._tensor;

    if (abs(eigenvectors(0,0).real())< abs(eigenvectors(0,1).real()))
    {
      eigenvectors(0,0) = eigenvectors(0,1) ;
      eigenvectors(1,0) = eigenvectors(1,1) ;

    }
    if (eigenvectors(0,0).real() < 0)
    {
        eigenvectors(0,0) = eigenvectors(0,0) * neg;
        eigenvectors(1,0) = eigenvectors(1,0) * neg;
    }
    eigenvectors(1,1) = eigenvectors(0,0);
    std::complex<double> temp((eigenvectors(1,0).real()) * -1,eigenvectors(1,0).imag()) ;
    eigenvectors(0,1) = temp;

    vector._tensor = eigenvectors;
    return vector;
    */
}

}  // namespace tensor

}  // namespace qsyn
