/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor decomposer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <ranges>
#include <tl/to.hpp>

#include "qcir/gate_type.hpp"
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

namespace tensor {

template <typename T>
struct TwoLevelMatrix {
    TwoLevelMatrix(QTensor<T> const& m, size_t i, size_t j) : _matrix(m), _i(i), _j(j) { assert(i < j); }
    QTensor<T> _matrix;
    size_t _i = 0, _j = 0;  // i < j
};

template <typename T>
TwoLevelMatrix<T> adjoint(TwoLevelMatrix<T> m /* copy on purpose */) {
    m._matrix = adjoint(m._matrix);
    return m;
}

template <typename T>
struct ZYZ {
    T phi;
    T alpha;
    T beta;  // actual beta/2
    T gamma;
    bool correct = true;
};

class Decomposer {
private:
    QCir _quantum_circuit;
    size_t _n_qubits = 0;

public:
    template <typename U>
    std::optional<QCir> decompose(QTensor<U> const& matrix);

private:
    template <typename U>
    TwoLevelMatrix<U> _make_two_level_matrix(QTensor<U> const& matrix, size_t i, size_t j) {
        auto const kernel =
            QTensor<U>({{matrix(i, i), matrix(i, j)},
                        {matrix(j, i), matrix(j, j)}});
        return TwoLevelMatrix<U>(kernel, i, j);
    }

    /**
     * @brief assumes that the input matrix is a square matrix
     *
     * @tparam U
     * @param matrix
     * @return size_t
     */
    template <typename U>
    size_t _get_dimension(Tensor<U> const& matrix) {
        DVLAB_ASSERT(matrix.shape().size() == 2 && matrix.shape()[0] == matrix.shape()[1], "Matrix is not square");
        return static_cast<size_t>(matrix.shape()[0]);
    }

    template <typename U>
    std::optional<std::pair<size_t, size_t>> _get_two_level_matrix_indices(QTensor<U> const& matrix, U eps);
    template <typename U>
    std::vector<TwoLevelMatrix<U>> _get_two_level_matrices(QTensor<U> matrix /* copy on purpose */);

    template <typename U>
    bool _graycode(Tensor<U> const& matrix, size_t i, size_t j);
    void _encode(size_t origin_pos, size_t targ_pos, std::vector<QubitIdList>& qubit_list, std::vector<std::string>& gate_list);
    void _encode_control_gate(QubitIdList const& target, std::vector<QubitIdList>& qubit_list, std::vector<std::string>& gate_list);

    template <typename U>
    bool _decompose_cnu(Tensor<U> const& t, size_t diff_pos, size_t index, size_t ctrl_gates);

    template <typename U>
    bool _decompose_cnx(const std::vector<size_t>& ctrls, size_t extract_qubit, size_t index, size_t ctrl_gates);

    template <typename U>
    bool _decompose_cu(Tensor<U> const& t, size_t ctrl, size_t targ);

    template <typename U>
    ZYZ<typename U::value_type> _decompose_zyz(Tensor<U> const& matrix);

    template <typename U>
    Tensor<U> _sqrt_single_qubit_matrix(Tensor<U> const& matrix);
};

/**
 * @brief Convert the matrix into a quantum circuit
 *
 * @tparam U
 * @param matrix
 * @return QCir*
 */
template <typename U>
std::optional<QCir> Decomposer::decompose(QTensor<U> const& matrix) {
    _n_qubits      = static_cast<size_t>(std::round(std::log2(_get_dimension(matrix))));
    auto mat_chain = _get_two_level_matrices(matrix);

    _quantum_circuit = QCir(_n_qubits);

    for (auto const& i : std::views::iota(0UL, mat_chain.size()) | std::views::reverse) {
        size_t i_idx = 0, j_idx = 0;
        for (size_t j = 0; j < _n_qubits; j++) {
            i_idx = i_idx * 2 + (mat_chain[i]._i >> j & 1);
            j_idx = j_idx * 2 + (mat_chain[i]._j >> j & 1);
        }
        if (i_idx > j_idx) {
            std::swap(i_idx, j_idx);
            std::swap(mat_chain[i]._matrix(0, 0), mat_chain[i]._matrix(1, 1));
            std::swap(mat_chain[i]._matrix(0, 1), mat_chain[i]._matrix(1, 0));
        }

        if (!_graycode(mat_chain[i]._matrix, i_idx, j_idx)) return std::nullopt;
    }
    return _quantum_circuit;
}

/**
 * @brief If the input matrix is a two-level matrix, return the indices of the two-level matrix
 *
 * @tparam U
 * @param matrix
 * @param eps
 * @return std::optional<std::pair<size_t, size_t>>
 */
template <typename U>
std::optional<std::pair<size_t, size_t>> Decomposer::_get_two_level_matrix_indices(QTensor<U> const& matrix, U eps) {
    using namespace std::literals;
    auto const dimension      = static_cast<size_t>(matrix.shape()[0]);
    size_t num_found_diagonal = 0, num_upper_triangle_not_zero = 0, num_lower_triangle_not_zero = 0;
    size_t top_main_diagonal_coords = 0, top_sub_diagonal_row = 0, bottom_sub_diagonal_row = 0;
    size_t bottom_main_diagonal_coords = 0, top_sub_diagonal_col = 0, bottom_sub_diagonal_col = 0;

    // count the number of non-1 elements in the diagonal
    // and the non-zero elements in the upper and lower triangles
    for (size_t x = 0; x < dimension; x++) {
        for (size_t y = 0; y < dimension; y++) {
            if (x == y) {
                if (std::abs(matrix(y, x) - (1.0 + 0.i)) > eps) {
                    num_found_diagonal++;
                    if (num_found_diagonal == 1)
                        top_main_diagonal_coords = x;
                    if (num_found_diagonal == 2)
                        bottom_main_diagonal_coords = x;
                }
            } else if (x > y) {  // Upper diagonal
                if (std::abs(matrix(y, x)) > eps) {
                    num_upper_triangle_not_zero++;
                    top_sub_diagonal_row = y;
                    top_sub_diagonal_col = x;
                }
            } else {  // Lower diagonal
                if (std::abs(matrix(y, x)) > eps) {
                    num_lower_triangle_not_zero++;
                    bottom_sub_diagonal_row = y;
                    bottom_sub_diagonal_col = x;
                }
            }
        }
    }

    // is the matrix a two-level one?

    // case 1: some off-diagonal elements are non-zero
    if ((num_upper_triangle_not_zero == 1 && num_lower_triangle_not_zero == 1) && (top_sub_diagonal_row == bottom_sub_diagonal_col && top_sub_diagonal_col == bottom_sub_diagonal_row)) {
        // kernel is [x x]
        //           [x x]
        if (num_found_diagonal == 2 && top_main_diagonal_coords == top_sub_diagonal_row && bottom_main_diagonal_coords == bottom_sub_diagonal_row) {
            return std::make_pair(top_main_diagonal_coords, bottom_main_diagonal_coords);
        }
        // REVIEW - why is this condition necessary?
        if (num_found_diagonal == 1 && (top_main_diagonal_coords == top_sub_diagonal_row || top_main_diagonal_coords == top_sub_diagonal_col)) {
            if (top_main_diagonal_coords != dimension - 1) {
                return std::make_pair(top_main_diagonal_coords, top_main_diagonal_coords + 1);
            } else {
                return std::make_pair(top_main_diagonal_coords - 1, top_main_diagonal_coords);
            }
        }
        // kernel is [0 x]
        //           [x 0]
        if (num_found_diagonal == 0) {
            return std::make_pair(top_sub_diagonal_row, top_sub_diagonal_col);
        }
    }

    // case 2: all off-diagonal elements are zero
    if (num_upper_triangle_not_zero == 0 && num_lower_triangle_not_zero == 0) {
        if (num_found_diagonal == 2) {
            return std::make_pair(top_main_diagonal_coords, bottom_main_diagonal_coords);
        }
        // REVIEW - why is this condition necessary?
        if (num_found_diagonal == 1) {
            if (top_main_diagonal_coords != dimension - 1) {
                return std::make_pair(top_main_diagonal_coords, top_main_diagonal_coords + 1);
            } else {
                return std::make_pair(top_main_diagonal_coords - 1, top_main_diagonal_coords);
            }
        }
        if (num_found_diagonal == 0)
            return std::make_pair(SIZE_MAX, SIZE_MAX);  // signals identity
        // if (num_found_diagonal == 0)  // identity
        //     return two_level_chain;
    }

    return std::nullopt;
}

/**
 * @brief Get the two-level matrices associated with the input matrix
 *
 * @tparam U
 * @param matrix
 * @return std::vector<TwoLevelMatrix<U>> List of two-level matrix
 * @reference Li, Chi-Kwong, Rebecca Roberts, and Xiaoyan Yin. "Decomposition of unitary matrices and quantum gates." International Journal of Quantum Information 11.01 (2013): 1350015.
 */
template <typename U>
std::vector<TwoLevelMatrix<U>> Decomposer::_get_two_level_matrices(QTensor<U> matrix /* copy on purpose */) {
    using namespace std::literals;
    constexpr U eps = 1e-6;
    std::vector<TwoLevelMatrix<U>> two_level_chain;
    auto const dimension = static_cast<size_t>(matrix.shape()[0]);

    for (size_t i = 0; i < dimension; i++) {
        for (size_t j = i + 1; j < dimension; j++) {
            // if `matrix` is the last two-level matrix
            if (auto const pair = _get_two_level_matrix_indices(matrix, eps)) {
                auto const& [selected_top, selected_bottom] = *pair;

                // shortcut for identity
                if (selected_top == SIZE_MAX && selected_bottom == SIZE_MAX) {
                    return two_level_chain;
                }

                two_level_chain.emplace_back(_make_two_level_matrix(matrix, selected_top, selected_bottom));
                return two_level_chain;
            }

            // not a two-level matrix

            // if encounter additional zeros in the off-diagonal elements, skip
            if (std::abs(matrix(i, i).real() - 1) < eps && std::abs(matrix(i, i).imag()) < eps) {  // maybe use e-6 approx.
                if (std::abs(matrix(j, i).real()) < eps && std::abs(matrix(j, i).imag()) < eps) {
                    continue;
                }
            }
            if (std::abs(matrix(i, i).real()) < eps && std::abs(matrix(i, i).imag()) < eps) {  // maybe use e-6 approx.
                if (std::abs(matrix(j, i).real()) < eps && std::abs(matrix(j, i).imag()) < eps) {
                    continue;
                }
            }

            // normalization factor
            const U u = std::sqrt(std::norm(matrix(i, i)) + std::norm(matrix(j, i)));

            auto const n_qubits = static_cast<size_t>(std::round(std::log2(_get_dimension(matrix))));
            QTensor<U> conjugate_matrix_product =
                QTensor<U>::identity(n_qubits).to_matrix(
                    std::views::iota(0ul, n_qubits) |
                        std::views::transform([](auto i) { return 2 * i + 1; }) |
                        tl::to<std::vector>(),
                    std::views::iota(0ul, n_qubits) |
                        std::views::transform([](auto i) { return 2 * i; }) |
                        tl::to<std::vector>());

            conjugate_matrix_product(i, i) = std::conj(matrix(i, i)) / u;
            conjugate_matrix_product(j, j) = matrix(i, i) / u;

            conjugate_matrix_product(i, j) = std::conj(matrix(j, i)) / u;
            conjugate_matrix_product(j, i) = -matrix(j, i) / u;

            matrix = tensordot(conjugate_matrix_product, matrix, {1}, {0});

            two_level_chain.emplace_back(adjoint(_make_two_level_matrix(conjugate_matrix_product, i, j)));
        }
    }

    return two_level_chain;
}

/**
 * @brief Perform Graycode synthesis
 *
 * @tparam U
 * @param matrix
 * @param I
 * @param J
 * @return true
 * @return false
 */
template <typename U>
bool Decomposer::Decomposer::_graycode(Tensor<U> const& matrix, size_t i, size_t j) {
    // do pabbing
    std::vector<QubitIdList> qubit_list;
    std::vector<std::string> gate_list;

    size_t diff_pos = 0;
    for (size_t q = 0; q < _n_qubits; q++) {
        if ((((i ^ j) >> q) & 1) && (j >> q & 1)) {
            diff_pos = q;
            break;
        }
    }

    if ((i + size_t(std::pow(2, diff_pos))) != size_t(std::pow(2, _n_qubits) - 1))
        _encode(i, diff_pos, qubit_list, gate_list);

    _encode(j, diff_pos, qubit_list, gate_list);

    // decompose CnU
    size_t ctrl_index = 0;  // q2 q1 q0 = t,c,c -> ctrl_index = 011 = 3
    for (size_t q = 0; q < _n_qubits; q++) {
        if (q != diff_pos)
            ctrl_index += size_t(pow(2, q));
    }
    if (!_decompose_cnu(matrix, diff_pos, ctrl_index, _n_qubits - 1)) return false;

    // do unpabbing
    DVLAB_ASSERT(gate_list.size() == qubit_list.size(), "Sizes of gate list and qubit list are different");
    for (auto const& q : std::views::iota(0UL, gate_list.size()) | std::views::reverse) {
        if (auto const op = qcir::str_to_operation(gate_list[q], {}); op.has_value()) {
            _quantum_circuit.append(*op, qubit_list[q]);
        }
    }

    return true;
}

/**
 * @brief Decompose the CnU gate
 *
 * @tparam U
 * @param t
 * @param diff_pos
 * @param index
 * @param ctrl_gates
 * @return true
 * @return false
 * @reference Nakahara, Mikio, and Tetsuo Ohmi. Quantum computing: from linear algebra to physical realizations. CRC press, 2008.
 */
template <typename U>
bool Decomposer::_decompose_cnu(Tensor<U> const& t, size_t diff_pos, size_t index, size_t ctrl_gates) {
    DVLAB_ASSERT(ctrl_gates >= 1, "The control qubit left in the CnU gate should be at least 1");
    size_t ctrl = (diff_pos == 0) ? 1 : diff_pos - 1;

    if (!((index >> ctrl) & 1)) {
        for (size_t i = 0; i < _n_qubits; i++) {
            if ((i == diff_pos) || (i == ctrl)) {
                continue;
            } else if ((index >> i) & 1) {
                ctrl = i;
                break;
            }
        }
    }
    if (ctrl_gates == 1) {
        if (!_decompose_cu(t, ctrl, diff_pos)) return false;
    } else {
        size_t extract_qubit = -1;
        for (size_t i = 0; i < _n_qubits; i++) {
            if (i == ctrl) continue;

            if ((index >> i) & 1) {
                extract_qubit = i;
                index         = index - size_t(pow(2, i));
                break;
            }
        }
        const Tensor<U> v = _sqrt_single_qubit_matrix(t);
        if (!_decompose_cu(v, extract_qubit, diff_pos)) return false;

        std::vector<size_t> ctrls;
        for (size_t i = 0; i < size_t(log2(index)) + 1; i++) {
            if ((index >> i) & 1)
                ctrls.emplace_back(i);
        }

        _decompose_cnx<U>(ctrls, extract_qubit, index, ctrl_gates - 1);

        if (!_decompose_cu(adjoint(v), extract_qubit, diff_pos)) return false;

        _decompose_cnx<U>(ctrls, extract_qubit, index, ctrl_gates - 1);

        if (!_decompose_cnu(v, diff_pos, index, ctrl_gates - 1)) return false;
    }

    return true;
}

/**
 * @brief Decompose CnX gate
 *
 * @tparam U
 * @param ctrls
 * @param extract_qubit
 * @param index
 * @param ctrl_gates
 * @return true
 * @return false
 */
template <typename U>
bool Decomposer::_decompose_cnx(const std::vector<size_t>& ctrls, const size_t extract_qubit, const size_t index, const size_t ctrl_gates) {
    if (ctrls.size() == 1) {
        _quantum_circuit.append(qcir::CXGate(), {gsl::narrow<QubitIdType>(ctrls[0]), gsl::narrow<QubitIdType>(extract_qubit)});
    } else if (ctrls.size() == 2) {
        _quantum_circuit.append(qcir::CCXGate(), {gsl::narrow<QubitIdType>(ctrls[0]), gsl::narrow<QubitIdType>(ctrls[1]), gsl::narrow<QubitIdType>(extract_qubit)});
    } else {
        using float_type = U::value_type;
        if (!_decompose_cnu(QTensor<float_type>::xgate(), extract_qubit, index, ctrl_gates)) return false;
    }
    return true;
}

/**
 * @brief Decompose CU gate
 *
 * @tparam U
 * @param t
 * @param ctrl
 * @param targ
 * @return true
 * @return false
 * @reference Nakahara, Mikio, and Tetsuo Ohmi. Quantum computing: from linear algebra to physical realizations. CRC press, 2008.
 */
template <typename U>
bool Decomposer::_decompose_cu(Tensor<U> const& t, size_t ctrl, size_t targ) {
    using dvlab::Phase;
    using float_type             = typename U::value_type;
    constexpr float_type eps     = 1e-6;
    ZYZ<float_type> const angles = _decompose_zyz(t);
    if (!angles.correct) return false;

    if (std::abs((angles.alpha - angles.gamma) / 2) > eps) {
        _quantum_circuit.append(qcir::RZGate(Phase{((angles.alpha - angles.gamma) / 2) * (-1.0)}), {gsl::narrow<QubitIdType>(targ)});
    }

    if (std::abs(angles.beta) > eps) {
        _quantum_circuit.append(qcir::CXGate(), {gsl::narrow<QubitIdType>(ctrl), gsl::narrow<QubitIdType>(targ)});
        if (std::abs((angles.alpha + angles.gamma) / 2) > eps) {
            _quantum_circuit.append(qcir::RZGate(Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}), {gsl::narrow<QubitIdType>(targ)});
        }

        _quantum_circuit.append(qcir::RYGate(Phase{angles.beta * (-1.0)}), {gsl::narrow<QubitIdType>(targ)});
        _quantum_circuit.append(qcir::CXGate(), {gsl::narrow<QubitIdType>(ctrl), gsl::narrow<QubitIdType>(targ)});
        _quantum_circuit.append(qcir::RYGate(Phase{angles.beta}), {gsl::narrow<QubitIdType>(targ)});

        if (std::abs(angles.alpha) > eps) {
            _quantum_circuit.append(qcir::RZGate(Phase(angles.alpha)), {gsl::narrow<QubitIdType>(targ)});
        }

    } else {
        if (std::abs((angles.alpha + angles.gamma) / 2) > eps) {
            _quantum_circuit.append(qcir::CXGate(), {gsl::narrow<QubitIdType>(ctrl), gsl::narrow<QubitIdType>(targ)});
            _quantum_circuit.append(qcir::RZGate(Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}), {gsl::narrow<QubitIdType>(targ)});
            _quantum_circuit.append(qcir::CXGate(), {gsl::narrow<QubitIdType>(ctrl), gsl::narrow<QubitIdType>(targ)});
        }
        if (std::abs(angles.alpha) > eps) {
            _quantum_circuit.append(qcir::RZGate(Phase(angles.alpha)), {gsl::narrow<QubitIdType>(targ)});
        }
    }
    if (std::abs(angles.phi) > eps) {
        _quantum_circuit.append(qcir::RZGate(Phase(angles.phi)), {gsl::narrow<QubitIdType>(ctrl)});
    }

    return true;
}

/**
 * @brief Decompose 2 by 2 matrix into RZ RY RZ gates
 *
 * @tparam U
 * @param matrix
 * @return ZYZ
 * @reference Nakahara, Mikio, and Tetsuo Ohmi. Quantum computing: from linear algebra to physical realizations. CRC press, 2008.
 */
template <typename U>
ZYZ<typename U::value_type> Decomposer::_decompose_zyz(Tensor<U> const& matrix) {
    DVLAB_ASSERT(matrix.shape()[0] == 2 && matrix.shape()[1] == 2, "decompose_ZYZ only supports 2x2 matrix");
    using namespace std::literals;
    using float_type = typename U::value_type;
    // a =  e^{iφ}e^{-i(α+γ)/2}cos(β/2)
    // b = -e^{iφ}e^{-i(α-γ)/2}sin(β/2)
    // c =  e^{iφ}e^{ i(α-γ)/2}sin(β/2)
    // d =  e^{iφ}e^{ i(α+γ)/2}cos(β/2)
    const U a = matrix(0, 0), b = matrix(0, 1), c = matrix(1, 0), d = matrix(1, 1);
    ZYZ<float_type> output = {};
    // NOTE - The beta here is actually half of beta
    float_type init_beta = 0;
    if (std::abs(a) > 1) {
        init_beta = 0;
    } else {
        init_beta = std::acos(std::abs(a));
    }

    constexpr auto pi = std::numbers::pi_v<float_type>;
    // NOTE - Possible betas due to arccosine
    const std::array<float_type, 4> beta_candidate = {init_beta, pi - init_beta, pi + init_beta, 2.0 * pi - init_beta};
    for (const auto& beta : beta_candidate) {
        output.beta = beta;
        U a1, b1, c1, d1;
        const U cos(std::cos(beta) + 1e-5, 0);  // cos(β/2)
        const U sin(std::sin(beta) + 1e-5, 0);  // sin(β/2)
        a1 = a / cos;
        b1 = b / sin;
        c1 = c / sin;
        d1 = d / cos;
        if (std::abs(b) < 1e-4) {
            output.alpha = std::arg(d1 / a1) / 2.0;
            output.gamma = output.alpha;
        } else if (std::abs(a) < 1e-4) {
            output.alpha = std::arg(-c1 / b1) / 2.0;
            output.gamma = (-1.0) * output.alpha;
        } else {
            output.alpha = std::arg(c1 / a1);
            output.gamma = std::arg(d1 / c1);
        }

        auto const alpha_plus_gamma  = std::exp(U((0.5i) * (output.alpha + output.gamma)));
        auto const alpha_minus_gamma = std::exp(U((0.5i) * (output.alpha - output.gamma)));

        if (std::abs(a) < 1e-4)
            output.phi = std::arg(c1 / alpha_minus_gamma);
        else
            output.phi = std::arg(a1 * alpha_plus_gamma);

        const U phi(std::cos(output.phi), std::sin(output.phi));

        if (std::abs(phi * cos / alpha_plus_gamma - a) < 1e-3 &&
            std::abs(sin * phi / alpha_minus_gamma + b) < 1e-3 &&
            std::abs(phi * alpha_minus_gamma * sin - c) < 1e-3 &&
            std::abs(phi * alpha_plus_gamma * cos - d) < 1e-3) {
            return output;
        }
    }
    output.correct = false;
    spdlog::error("No solution to ZYZ decomposition");

    return output;
}

/**
 * @brief Generate the square root of a 2 by 2 matrix
 *
 * @tparam U
 * @param matrix
 * @return Tensor<U>
 * @reference https://en.wikipedia.org/wiki/Square_root_of_a_2_by_2_matrix
 */
template <typename U>
Tensor<U> Decomposer::_sqrt_single_qubit_matrix(Tensor<U> const& matrix) {
    DVLAB_ASSERT(matrix.shape()[0] == 2 && matrix.shape()[1] == 2, "sqrt_single_qubit_matrix only supports 2x2 matrix");
    // a b
    // c d
    const U a = matrix(0, 0), b = matrix(0, 1), c = matrix(1, 0), d = matrix(1, 1);
    const U tau = a + d, delta = a * d - b * c;
    const std::complex s = std::sqrt(delta);
    const std::complex t = std::sqrt(tau + 2. * s);
    if (std::abs(t) > 0) {
        return Tensor<U>({{(a + s) / t, b / t}, {c / t, (d + s) / t}});
    } else {
        // Diagonalized matrix
        return Tensor<U>({{std::sqrt(a), b}, {c, std::sqrt(d)}});
    }
}

}  // namespace tensor

}  // namespace qsyn
