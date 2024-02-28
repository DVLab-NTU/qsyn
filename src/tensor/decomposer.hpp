/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <ranges>

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
    TwoLevelMatrix(QTensor<T> const& m, size_t i, size_t j) : _matrix(m), _i(i), _j(j) {}
    QTensor<T> _matrix;
    size_t _i = 0, _j = 0;  // i < j
};

struct ZYZ {
    double phi;
    double alpha;
    double beta;  // actual beta/2
    double gamma;
    bool correct = true;
};

class Decomposer {
public:
    template <typename U>
    friend struct TwoLevelMatrix;
    friend struct ZYZ;

    Decomposer(size_t reg) : _quantum_circuit(new QCir(reg)), _qreg(reg) {
    }

    QCir* get_qcir() { return _quantum_circuit; }
    size_t get_qreg() { return _qreg; }

    /**
     * @brief Convert the matrix into a quantum circuit
     *
     * @tparam U
     * @param matrix
     * @return QCir*
     */
    template <typename U>
    QCir* decompose(QTensor<U> const& matrix) {
        auto mat_chain = get_2level(matrix);

        for (auto const& i : std::views::iota(0UL, mat_chain.size()) | std::views::reverse) {
            size_t i_idx = 0, j_idx = 0;
            for (size_t j = 0; j < _qreg; j++) {
                i_idx = i_idx * 2 + (mat_chain[i]._i >> j & 1);
                j_idx = j_idx * 2 + (mat_chain[i]._j >> j & 1);
            }
            if (i_idx > j_idx) {
                std::swap(i_idx, j_idx);
                std::swap(mat_chain[i]._matrix(0, 0), mat_chain[i]._matrix(1, 1));
                std::swap(mat_chain[i]._matrix(0, 1), mat_chain[i]._matrix(1, 0));
            }

            if (!graycode(mat_chain[i]._matrix, i_idx, j_idx)) return nullptr;
        }
        return _quantum_circuit;
    }

    /**
     * @brief Get the 2level object
     *
     * @tparam U
     * @param matrix
     * @return std::vector<TwoLevelMatrix<U>> List of two-level matrix
     * @reference Li, Chi-Kwong, Rebecca Roberts, and Xiaoyan Yin. "Decomposition of unitary matrices and quantum gates." International Journal of Quantum Information 11.01 (2013): 1350015.
     */
    template <typename U>
    std::vector<TwoLevelMatrix<U>> get_2level(QTensor<U> matrix) {
        std::vector<TwoLevelMatrix<U>> two_level_chain;
        using namespace std::literals;
        const size_t dimension = size_t(matrix.shape()[0]);
        QTensor<U>
            conjugate_matrix_product = QTensor<U>::identity((size_t)round(std::log2(dimension)));
        conjugate_matrix_product.reshape({matrix.shape()[0], matrix.shape()[0]});

        QTensor<U>
            two_level_kernel = QTensor<U>::identity(1);

        for (size_t i = 0; i < dimension; i++) {
            for (size_t j = i + 1; j < dimension; j++) {
                // check 2-level
                bool is_two_level         = false;
                size_t num_found_diagonal = 0, num_upper_triangle_not_zero = 0, num_lower_triangle_not_zero = 0;
                size_t top_main_diagonal_coords = 0, top_sub_diagonal_row = 0, bottom_sub_diagonal_row = 0;
                size_t bottom_main_diagonal_coords = 0, top_sub_diagonal_col = 0, bottom_sub_diagonal_col = 0;
                size_t selected_top = 0, selected_bottom = 0;

                for (size_t x = 0; x < dimension; x++) {
                    for (size_t y = 0; y < dimension; y++) {
                        if (x == y) {
                            if (std::abs(matrix(y, x) - std::complex(1.0, 0.)) > 1e-6) {
                                num_found_diagonal++;
                                if (num_found_diagonal == 1)
                                    top_main_diagonal_coords = x;
                                if (num_found_diagonal == 2)
                                    bottom_main_diagonal_coords = x;
                            }
                        } else if (x > y) {  // Upper diagonal
                            if (std::abs(matrix(y, x)) > 1e-6) {
                                num_upper_triangle_not_zero++;
                                top_sub_diagonal_row = y;
                                top_sub_diagonal_col = x;
                            }
                        } else {  // Lower diagonal
                            if (std::abs(matrix(y, x)) > 1e-6) {
                                num_lower_triangle_not_zero++;
                                bottom_sub_diagonal_row = y;
                                bottom_sub_diagonal_col = x;
                            }
                        }
                    }
                }

                if ((num_upper_triangle_not_zero == 1 && num_lower_triangle_not_zero == 1) && (top_sub_diagonal_row == bottom_sub_diagonal_col && top_sub_diagonal_col == bottom_sub_diagonal_row)) {
                    if (num_found_diagonal == 2 && top_main_diagonal_coords == top_sub_diagonal_row && bottom_main_diagonal_coords == bottom_sub_diagonal_row) {
                        is_two_level    = true;
                        selected_top    = top_main_diagonal_coords;
                        selected_bottom = bottom_main_diagonal_coords;
                    }
                    if (num_found_diagonal == 1 && (top_main_diagonal_coords == top_sub_diagonal_row || top_main_diagonal_coords == top_sub_diagonal_col)) {
                        is_two_level = true;
                        if (top_main_diagonal_coords != dimension - 1) {
                            selected_top    = top_main_diagonal_coords;
                            selected_bottom = top_main_diagonal_coords + 1;
                        } else {
                            selected_top    = top_main_diagonal_coords - 1;
                            selected_bottom = top_main_diagonal_coords;
                        }
                    }
                    if (num_found_diagonal == 0) {
                        is_two_level    = true;
                        selected_top    = top_sub_diagonal_row;
                        selected_bottom = top_sub_diagonal_col;
                    }
                }

                if (num_upper_triangle_not_zero == 0 && num_lower_triangle_not_zero == 0) {
                    if (num_found_diagonal == 2) {
                        is_two_level    = true;
                        selected_top    = top_main_diagonal_coords;
                        selected_bottom = bottom_main_diagonal_coords;
                    }
                    if (num_found_diagonal == 1) {
                        is_two_level = true;
                        if (top_main_diagonal_coords != dimension - 1) {
                            selected_top    = top_main_diagonal_coords;
                            selected_bottom = top_main_diagonal_coords + 1;
                        } else {
                            selected_top    = top_main_diagonal_coords - 1;
                            selected_bottom = top_main_diagonal_coords;
                        }
                    }
                    if (num_found_diagonal == 0)  // identity
                        return two_level_chain;
                }

                if (is_two_level == true) {
                    two_level_kernel(0, 0) = matrix(selected_top, selected_top);
                    two_level_kernel(0, 1) = matrix(selected_top, selected_bottom);
                    two_level_kernel(1, 0) = matrix(selected_bottom, selected_top);
                    two_level_kernel(1, 1) = matrix(selected_bottom, selected_bottom);
                    two_level_chain.emplace_back(TwoLevelMatrix<U>(two_level_kernel, selected_top, selected_bottom));

                    return two_level_chain;
                }

                if (std::abs(matrix(i, i).real() - 1) < 1e-6 && std::abs(matrix(i, i).imag()) < 1e-6) {  // maybe use e-6 approx.
                    if (std::abs(matrix(j, i).real()) < 1e-6 && std::abs(matrix(j, i).imag()) < 1e-6) {
                        continue;
                    }
                }
                if (std::abs(matrix(i, i).real()) < 1e-6 && std::abs(matrix(i, i).imag()) < 1e-6) {  // maybe use e-6 approx.
                    if (std::abs(matrix(j, i).real()) < 1e-6 && std::abs(matrix(j, i).imag()) < 1e-6) {
                        continue;
                    }
                }

                const double u = std::sqrt(std::norm(matrix(i, i)) + std::norm(matrix(j, i)));

                using namespace std::literals;

                for (size_t x = 0; x < dimension; x++) {
                    for (size_t y = 0; y < dimension; y++) {
                        if (x == y) {
                            if (x == i) {
                                conjugate_matrix_product(x, y) = (std::conj(matrix(i, i))) / u;
                            } else if (x == j) {
                                conjugate_matrix_product(x, y) = matrix(i, i) / u;
                            } else {
                                conjugate_matrix_product(x, y) = 1.0 + 0.i;
                            }
                        } else if (x == j && y == i) {
                            conjugate_matrix_product(x, y) = (-1. + 0.i) * matrix(j, i) / u;
                        } else if (x == i && y == j) {
                            conjugate_matrix_product(x, y) = (std::conj(matrix(j, i))) / u;
                        } else {
                            conjugate_matrix_product(x, y) = 0. + 0.i;
                        }
                    }
                }

                matrix = tensordot(conjugate_matrix_product, matrix, {1}, {0});

                two_level_kernel(0, 0) = std::conj(conjugate_matrix_product(i, i));
                two_level_kernel(0, 1) = std::conj(conjugate_matrix_product(j, i));
                two_level_kernel(1, 0) = std::conj(conjugate_matrix_product(i, j));
                two_level_kernel(1, 1) = std::conj(conjugate_matrix_product(j, j));

                two_level_chain.emplace_back(TwoLevelMatrix<U>(two_level_kernel, i, j));
            }
        }

        return two_level_chain;
    }

    /**
     * @brief Append gate and save the encoding gate sequence
     *
     * @param target
     * @param qubit_list
     * @param gate_list
     */
    void encode_control_gate(const QubitIdList target, std::vector<QubitIdList>& qubit_list, std::vector<std::string>& gate_list) {
        qubit_list.emplace_back(target);
        gate_list.emplace_back(target.size() == 2 ? "cx" : "x");
        _quantum_circuit->add_gate(target.size() == 2 ? "cx" : "x", target, {}, true);
    }

    /**
     * @brief Gray encoder
     *
     * @param origin_pos Origin qubit
     * @param targ_pos Target qubit
     * @param qubit_list
     * @param gate_list
     */
    void encode(size_t origin_pos, size_t targ_pos, std::vector<QubitIdList>& qubit_list, std::vector<std::string>& gate_list) {
        bool x_given = 0;
        if (((origin_pos >> targ_pos) & 1) == 0) {
            encode_control_gate({int(targ_pos)}, qubit_list, gate_list);
            x_given = 1;
        }
        for (size_t i = 0; i < _qreg; i++) {
            if (i == targ_pos) continue;
            if (((origin_pos >> i) & 1) == 0)
                encode_control_gate({int(targ_pos), int(i)}, qubit_list, gate_list);
        }
        if (x_given)
            encode_control_gate({int(targ_pos)}, qubit_list, gate_list);
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
    bool graycode(Tensor<U> const& matrix, size_t I, size_t J) {
        // do pabbing
        std::vector<QubitIdList> qubit_list;
        std::vector<std::string> gate_list;

        size_t diff_pos = 0;
        for (size_t i = 0; i < _qreg; i++) {
            if ((((I ^ J) >> i) & 1) && (J >> i & 1)) {
                diff_pos = i;
                break;
            }
        }

        if ((I + size_t(std::pow(2, diff_pos))) != size_t(std::pow(2, _qreg) - 1))
            encode(I, diff_pos, qubit_list, gate_list);

        encode(J, diff_pos, qubit_list, gate_list);

        // decompose CnU
        size_t ctrl_index = 0;  // q2 q1 q0 = t,c,c -> ctrl_index = 011 = 3
        for (size_t i = 0; i < _qreg; i++) {
            if (i != diff_pos)
                ctrl_index += size_t(pow(2, i));
        }
        if (!decompose_CnU(matrix, diff_pos, ctrl_index, _qreg - 1)) return false;

        // do unpabbing
        DVLAB_ASSERT(gate_list.size() == qubit_list.size(), "Sizes of gate list and qubit list are different");
        for (auto const& i : std::views::iota(0UL, gate_list.size()) | std::views::reverse)
            _quantum_circuit->add_gate(gate_list[i], qubit_list[i], {}, true);

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
    bool decompose_CnU(Tensor<U> const& t, size_t diff_pos, size_t index, size_t ctrl_gates) {
        DVLAB_ASSERT(ctrl_gates >= 1, "The control qubit left in the CnU gate should be at least 1");
        size_t ctrl = (diff_pos == 0) ? 1 : diff_pos - 1;

        if (!((index >> ctrl) & 1)) {
            for (size_t i = 0; i < _qreg; i++) {
                if ((i == diff_pos) || (i == ctrl)) {
                    continue;
                } else if ((index >> i) & 1) {
                    ctrl = i;
                    break;
                }
            }
        }
        if (ctrl_gates == 1) {
            if (!decompose_CU(t, ctrl, diff_pos)) return false;
        } else {
            size_t extract_qubit = -1;
            for (size_t i = 0; i < _qreg; i++) {
                if (i == ctrl) continue;

                if ((index >> i) & 1) {
                    extract_qubit = i;
                    index         = index - size_t(pow(2, i));
                    break;
                }
            }
            Tensor<U> V = sqrt_single_qubit_matrix(t);
            if (!decompose_CU(V, extract_qubit, diff_pos)) return false;

            std::vector<size_t> ctrls;
            for (size_t i = 0; i < size_t(log2(index)) + 1; i++) {
                if ((index >> i) & 1)
                    ctrls.emplace_back(i);
            }

            decompose_CnX<U>(ctrls, extract_qubit, index, ctrl_gates - 1);

            V.adjoint();
            if (!decompose_CU(V, extract_qubit, diff_pos)) return false;

            decompose_CnX<U>(ctrls, extract_qubit, index, ctrl_gates - 1);

            V.adjoint();
            if (!decompose_CnU(V, diff_pos, index, ctrl_gates - 1)) return false;
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
    bool decompose_CnX(const std::vector<size_t>& ctrls, const size_t extract_qubit, const size_t index, const size_t ctrl_gates) {
        if (ctrls.size() == 1) {
            _quantum_circuit->add_gate("cx", {int(ctrls[0]), int(extract_qubit)}, {}, true);
        } else if (ctrls.size() == 2) {
            _quantum_circuit->add_gate("ccx", {int(ctrls[0]), int(ctrls[1]), int(extract_qubit)}, {}, true);
        } else {
            using float_type = U::value_type;
            if (!decompose_CnU(QTensor<float_type>::xgate(), extract_qubit, index, ctrl_gates)) return false;
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
    bool decompose_CU(Tensor<U> const& t, size_t ctrl, size_t targ) {
        const struct ZYZ angles = decompose_ZYZ(t);
        if (!angles.correct) return false;

        if (std::abs((angles.alpha - angles.gamma) / 2) > 1e-6)
            _quantum_circuit->add_gate("rz", {int(targ)}, dvlab::Phase{((angles.alpha - angles.gamma) / 2) * (-1.0)}, true);

        if (std::abs(angles.beta) > 1e-6) {
            _quantum_circuit->add_gate("cx", {int(ctrl), int(targ)}, {}, true);
            if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6)
                _quantum_circuit->add_gate("rz", {int(targ)}, dvlab::Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}, true);

            _quantum_circuit->add_gate("ry", {int(targ)}, dvlab::Phase{angles.beta * (-1.0)}, true);
            _quantum_circuit->add_gate("cx", {int(ctrl), int(targ)}, {}, true);
            _quantum_circuit->add_gate("ry", {int(targ)}, dvlab::Phase{angles.beta}, true);

            if (std::abs(angles.alpha) > 1e-6)
                _quantum_circuit->add_gate("rz", {int(targ)}, dvlab::Phase{angles.alpha}, true);

        } else {
            if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6) {
                _quantum_circuit->add_gate("cx", {int(ctrl), int(targ)}, {}, true);
                _quantum_circuit->add_gate("rz", {int(targ)}, dvlab::Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}, true);
                _quantum_circuit->add_gate("cx", {int(ctrl), int(targ)}, {}, true);
            }
            if (std::abs(angles.alpha) > 1e-6)
                _quantum_circuit->add_gate("rz", {int(targ)}, dvlab::Phase{angles.alpha}, true);
        }
        if (std::abs(angles.phi) > 1e-6)
            _quantum_circuit->add_gate("rz", {int(ctrl)}, dvlab::Phase{angles.phi}, true);

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
    ZYZ decompose_ZYZ(Tensor<U> const& matrix) {
        DVLAB_ASSERT(matrix.shape()[0] == 2 && matrix.shape()[1] == 2, "decompose_ZYZ only supports 2x2 matrix");
        using namespace std::literals;
        // NOTE - // e^(iφαβγ)
        // a =  e^{iφ}e^{-i(α+γ)/2}cos(β/2)
        // b = -e^{iφ}e^{-i(α-γ)/2}sin(β/2)
        // c =  e^{iφ}e^{ i(α-γ)/2}sin(β/2)
        // d =  e^{iφ}e^{ i(α+γ)/2}cos(β/2)
        const std::complex<double> a = matrix(0, 0), b = matrix(0, 1), c = matrix(1, 0), d = matrix(1, 1);
        struct ZYZ output = {};
        // NOTE - The beta here is actually half of beta
        double init_beta = 0;
        if (std::abs(a) > 1) {
            init_beta = 0;
        } else {
            init_beta = std::acos(std::abs(a));
        }

        // NOTE - Possible betas due to arccosine
        const std::array<double, 4> beta_candidate = {init_beta, PI - init_beta, PI + init_beta, 2.0 * PI - init_beta};
        for (const auto& beta : beta_candidate) {
            output.beta = beta;
            std::complex<double> a1, b1, c1, d1;
            const std::complex<double> cos(std::cos(beta) + 1e-5, 0);  // cos(β/2)
            const std::complex<double> sin(std::sin(beta) + 1e-5, 0);  // sin(β/2)
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

            auto const alpha_plus_gamma  = std::exp(std::complex<double>((0.5i) * (output.alpha + output.gamma)));
            auto const alpha_minus_gamma = std::exp(std::complex<double>((0.5i) * (output.alpha - output.gamma)));

            if (std::abs(a) < 1e-4)
                output.phi = std::arg(c1 / alpha_minus_gamma);
            else
                output.phi = std::arg(a1 * alpha_plus_gamma);

            const std::complex<double> phi(std::cos(output.phi), std::sin(output.phi));

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
    Tensor<U> sqrt_single_qubit_matrix(Tensor<U> const& matrix) {
        DVLAB_ASSERT(matrix.shape()[0] == 2 && matrix.shape()[1] == 2, "sqrt_single_qubit_matrix only supports 2x2 matrix");
        // a b
        // c d
        const std::complex<double> a = matrix(0, 0), b = matrix(0, 1), c = matrix(1, 0), d = matrix(1, 1);
        const std::complex<double> tau = a + d, delta = a * d - b * c;
        const std::complex s = std::sqrt(delta);
        const std::complex t = std::sqrt(tau + 2. * s);
        if (std::abs(t) > 0) {
            return Tensor<U>({{(a + s) / t, b / t}, {c / t, (d + s) / t}});
        } else {
            // Diagonalized matrix
            return Tensor<U>({{std::sqrt(a), b}, {c, std::sqrt(d)}});
        }
    }

private:
    QCir* _quantum_circuit;
    size_t _qreg;
};

}  // namespace tensor

}  // namespace qsyn
