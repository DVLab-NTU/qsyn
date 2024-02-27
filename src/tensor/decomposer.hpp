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

namespace qsyn {

using qcir::QCir;
using tensor::QTensor;
using tensor::Tensor;

namespace decomposer {

template <typename T>

struct TwoLevelMatrix {
    QTensor<T> given;
    size_t i = 0, j = 0;  // i < j
    TwoLevelMatrix(QTensor<T> const& _mat) : given(_mat) {}
};

struct ZYZ {
    double phi;
    double alpha;
    double beta;  // actual beta/2
    double gamma;
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

    template <typename U>
    QCir* decompose(QTensor<U> const& matrix) {
        auto mat_chain = get_2level(matrix);

        // size_t end = mat_chain.size();

        for (auto const& i : std::views::iota(0UL, mat_chain.size()) | std::views::reverse) {
            size_t i_idx = 0, j_idx = 0;
            for (size_t j = 0; j < _qreg; j++) {
                i_idx = i_idx * 2 + (mat_chain[i].i >> j & 1);
                j_idx = j_idx * 2 + (mat_chain[i].j >> j & 1);
            }
            if (i_idx > j_idx) {
                std::swap(i_idx, j_idx);
                std::swap(mat_chain[i].given(0, 0), mat_chain[i].given(1, 1));
                std::swap(mat_chain[i].given(0, 1), mat_chain[i].given(1, 0));
            }
            // fmt::println("original: {}, {}; after: {}, {}", two_level_chain[i].i, two_level_chain[i].j, i_idx, j_idx);

            graycode(mat_chain[i].given, i_idx, j_idx);
        }

        // for (size_t i = end - 1; i >= 0; i--) {
        //     size_t i_idx = 0, j_idx = 0;
        //     for (size_t j = 0; j < _qreg; j++) {
        //         i_idx = i_idx * 2 + (mat_chain[i].i >> j & 1);
        //         j_idx = j_idx * 2 + (mat_chain[i].j >> j & 1);
        //     }
        //     if (i_idx > j_idx) {
        //         std::swap(i_idx, j_idx);
        //         std::swap(mat_chain[i].given(0, 0), mat_chain[i].given(1, 1));
        //         std::swap(mat_chain[i].given(0, 1), mat_chain[i].given(1, 0));
        //     }
        //     // fmt::println("original: {}, {}; after: {}, {}", two_level_chain[i].i, two_level_chain[i].j, i_idx, j_idx);

        //     graycode(mat_chain[i].given, i_idx, j_idx);
        //     // graycode(two_level_chain[i].given, two_level_chain[i].i, two_level_chain[i].j, qreg, fs);
        // }
        return _quantum_circuit;
    }

    template <typename U>
    auto get_2level(QTensor<U> t) {
        std::vector<TwoLevelMatrix<U>> two_level_chain;

        using namespace std::literals;

        const size_t s = size_t(t.shape()[0]);

        QTensor<U>
            XU = QTensor<U>::identity((size_t)round(std::log2(s)));

        XU.reshape({t.shape()[0], t.shape()[0]});

        QTensor<U>
            FU = QTensor<U>::identity(1);

        for (size_t i = 0; i < s; i++) {
            for (size_t j = i + 1; j < s; j++) {
                // check 2-level
                bool is_two_level = false;
                size_t d = 0, Up = 0, L = 0;
                size_t d1 = 0, Ui = 0, Li = 0;
                size_t d2 = 0, Uj = 0, Lj = 0;
                size_t c_i = 0, c_j = 0;
                // std::complex one(1.0, 0.);

                for (size_t x = 0; x < s; x++) {  // check all
                    for (size_t y = 0; y < s; y++) {
                        if (x == y) {
                            if (std::abs(t(y, x) - std::complex(1.0, 0.)) > 1e-6) {
                                d++;
                                if (d == 1) {
                                    d1 = x;
                                }
                                if (d == 2) {
                                    d2 = x;
                                }
                            }
                        } else if (x > y) {
                            if (std::abs(t(y, x)) > 1e-6) {
                                Up++;
                                Ui = y;
                                Uj = x;
                            }
                        } else {
                            if (std::abs(t(y, x)) > 1e-6) {
                                L++;
                                Li = y;
                                Lj = x;
                            }
                        }
                    }
                }

                if ((Up == 1 && L == 1) && (Ui == Lj && Uj == Li)) {
                    if (d == 2 && d1 == Ui && d2 == Li) {
                        is_two_level = true;
                        c_i          = d1;
                        c_j          = d2;
                    }
                    if (d == 1 && (d1 == Ui || d1 == Uj)) {
                        is_two_level = true;
                        if (d1 != s - 1) {
                            c_i = d1;
                            c_j = d1 + 1;
                        } else {
                            c_i = d1 - 1;
                            c_j = d1;
                        }
                    }
                    if (d == 0) {
                        is_two_level = true;
                        c_i          = Ui;
                        c_j          = Uj;
                    }
                }

                if (Up == 0 && L == 0) {
                    if (d == 2) {
                        is_two_level = true;
                        c_i          = d1;
                        c_j          = d2;
                    }
                    if (d == 1) {
                        is_two_level = true;
                        if (d1 != s - 1) {
                            c_i = d1;
                            c_j = d1 + 1;
                        } else {
                            c_i = d1 - 1;
                            c_j = d1;
                        }
                    }
                    if (d == 0) {  // identity
                        // fmt::println("U become I, ended");
                        return two_level_chain;
                    }
                }

                if (is_two_level == true) {
                    FU(0, 0) = t(c_i, c_i);
                    FU(0, 1) = t(c_i, c_j);
                    FU(1, 0) = t(c_j, c_i);
                    FU(1, 1) = t(c_j, c_j);
                    TwoLevelMatrix<U> m(FU);
                    m.i = c_i;
                    m.j = c_j;
                    two_level_chain.push_back(m);

                    return two_level_chain;
                }

                if (std::abs(t(i, i).real() - 1) < 1e-6 && std::abs(t(i, i).imag()) < 1e-6) {  // maybe use e-6 approx.
                    if (std::abs(t(j, i).real()) < 1e-6 && std::abs(t(j, i).imag()) < 1e-6) {
                        continue;
                    }
                }
                if (std::abs(t(i, i).real()) < 1e-6 && std::abs(t(i, i).imag()) < 1e-6) {  // maybe use e-6 approx.
                    if (std::abs(t(j, i).real()) < 1e-6 && std::abs(t(j, i).imag()) < 1e-6) {
                        continue;
                    }
                }

                const double u = std::sqrt(std::norm(t(i, i)) + std::norm(t(j, i)));

                using namespace std::literals;

                for (size_t x = 0; x < s; x++) {
                    for (size_t y = 0; y < s; y++) {
                        if (x == y) {
                            if (x == i) {
                                XU(x, y) = (std::conj(t(i, i))) / u;
                            } else if (x == j) {
                                XU(x, y) = t(i, i) / u;
                            } else {
                                XU(x, y) = 1.0 + 0.i;
                            }
                        } else if (x == j && y == i) {
                            XU(x, y) = (-1. + 0.i) * t(j, i) / u;
                        } else if (x == i && y == j) {
                            XU(x, y) = (std::conj(t(j, i))) / u;
                        } else {
                            XU(x, y) = 0. + 0.i;
                        }
                    }
                }

                t = tensordot(XU, t, {1}, {0});

                QTensor<double>
                    CU = QTensor<double>::identity(1);

                CU(0, 0) = std::conj(XU(i, i));
                CU(0, 1) = std::conj(XU(j, i));
                CU(1, 0) = std::conj(XU(i, j));
                CU(1, 1) = std::conj(XU(j, j));

                TwoLevelMatrix<U> m(CU);
                m.i = i;
                m.j = j;
                two_level_chain.push_back(m);
            }
        }

        return two_level_chain;
    }

    template <typename U>
    size_t graycode(Tensor<U> const& t, int I, int J) {
        // fmt::println("in graycode function");
        // do pabbing
        std::vector<QubitIdList> qubits_list;
        std::vector<std::string> gate_list;
        size_t gates_length = 0;

        // size_t get_diff = I ^ J;
        size_t diff_pos = 0;
        for (size_t i = 0; i < _qreg; i++) {
            if ((((I ^ J) >> i) & 1) && (J >> i & 1)) {
                diff_pos = i;
                break;
            }
        }

        bool x_given = 0;

        if ((I + std::pow(2, diff_pos)) != (std::pow(2, _qreg) - 1)) {
            if (((I >> diff_pos) & 1) == 0) {
                QubitIdList target;
                target.emplace_back(diff_pos);
                qubits_list.emplace_back(target);
                gate_list.emplace_back("x");
                gates_length++;
                _quantum_circuit->add_gate("x", target, {}, true);
                x_given = 1;
            }
            for (size_t i = 0; i < _qreg; i++) {
                if (i == diff_pos) {
                    continue;
                }
                if (((I >> i) & 1) == 0) {
                    QubitIdList target;
                    target.emplace_back(diff_pos);
                    target.emplace_back(i);
                    qubits_list.emplace_back(target);
                    gate_list.emplace_back("cx");
                    gates_length++;
                    _quantum_circuit->add_gate("cx", target, {}, true);
                    // std::cout << "cx on ctrl: " << diff_pos << " targ: " << i << std::endl;
                }
            }
            if (x_given) {
                QubitIdList target;
                target.emplace_back(diff_pos);
                qubits_list.emplace_back(target);
                gate_list.emplace_back("x");
                gates_length++;
                _quantum_circuit->add_gate("x", target, {}, true);

                // std::cout << "x on " << diff_pos << std::endl;
                x_given = 0;
            }
        }
        if (((J >> diff_pos) & 1) == 0) {
            QubitIdList target;
            target.emplace_back(diff_pos);
            qubits_list.emplace_back(target);
            gate_list.emplace_back("x");
            gates_length++;
            _quantum_circuit->add_gate("x", target, {}, true);
            // std::cout << "x on " << diff_pos << std::endl;
            x_given = 1;
        }
        for (size_t i = 0; i < _qreg; i++) {
            if (i == diff_pos) {
                continue;
            }
            if (((J >> i) & 1) == 0) {
                QubitIdList target;
                target.emplace_back(diff_pos);
                target.emplace_back(i);
                qubits_list.emplace_back(target);
                gate_list.emplace_back("cx");
                gates_length++;
                _quantum_circuit->add_gate("cx", target, {}, true);
            }
        }
        if (x_given) {
            QubitIdList target;
            target.emplace_back(diff_pos);
            qubits_list.emplace_back(target);
            gate_list.emplace_back("x");
            gates_length++;
            _quantum_circuit->add_gate("x", target, {}, true);
            // std::cout << "x on " << diff_pos << std::endl;
            x_given = 0;
        }
        // decompose CnU
        size_t ctrl_index = 0;
        for (size_t i = 0; i < _qreg; i++) {
            if (i != diff_pos) {
                ctrl_index += size_t(pow(2, i));
            }
        }
        decompose_CnU(t, diff_pos, ctrl_index, _qreg - 1);

        // do unpabbing
        for (auto const& i : std::views::iota(0UL, gates_length) | std::views::reverse) {
            _quantum_circuit->add_gate(gate_list[i], qubits_list[i], {}, true);
        }
        // for (size_t i = gates_length - 1; i >= 0; i--) {
        //     _quantum_circuit->add_gate(gate_list[i], qubits_list[i], {}, true);
        // }
        return diff_pos;
    }

    // REVIEW - ctrl_gates >= 1
    template <typename U>
    int decompose_CnU(Tensor<U> const& t, size_t diff_pos, size_t index, size_t ctrl_gates) {
        // fmt::println("in decompose to decompose CnU function ctrl_gates: {}", t);
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
            decompose_CU(t, ctrl, diff_pos);
        } else {
            size_t extract_qubit = -1;
            for (size_t i = 0; i < _qreg; i++) {
                if (i == ctrl) {
                    continue;
                }
                if ((index >> i) & 1) {
                    extract_qubit = i;
                    index         = index - size_t(pow(2, i));
                    break;
                }
            }
            Tensor<U> V = sqrt_tensor(t);
            // std::cout << "extract qubit: " << extract_qubit << std::endl;
            decompose_CU(V, extract_qubit, diff_pos);
            // size_t max_index = (size_t(log2(index)) + 1);
            size_t count = 0;
            std::vector<size_t> ctrls;
            for (size_t i = 0; i < size_t(log2(index)) + 1; i++) {
                if ((index >> i) & 1) {
                    ctrls.emplace_back(i);
                    count++;
                }
            }
            if (count == 1) {
                QubitIdList target;
                target.emplace_back(ctrls[0]);
                target.emplace_back(extract_qubit);
                _quantum_circuit->add_gate("cx", target, {}, true);
            } else if (count == 2) {
                QubitIdList target;
                for (size_t i = 0; i < count; i++) {
                    target.emplace_back(ctrls[i]);
                }
                target.emplace_back(extract_qubit);
                _quantum_circuit->add_gate("ccx", target, {}, true);
            } else {
                std::complex<double> zero(0, 0), one(1, 0);
                // Tensor<U> x({{zero, one}, {one, zero}});
                decompose_CnU(Tensor<U>({{zero, one}, {one, zero}}), extract_qubit, index, ctrl_gates - 1);
            }
            V.adjoint();
            decompose_CU(V, extract_qubit, diff_pos);
            if (count == 1) {
                QubitIdList target;
                target.emplace_back(ctrls[0]);
                target.emplace_back(extract_qubit);
                _quantum_circuit->add_gate("cx", target, {}, true);
            } else if (count == 2) {
                QubitIdList target;
                for (size_t i = 0; i < count; i++) {
                    target.emplace_back(ctrls[i]);
                }
                target.emplace_back(extract_qubit);
                _quantum_circuit->add_gate("ccx", target, {}, true);
            } else {
                std::complex<double> zero(0, 0), one(1, 0);
                // Tensor<U> x({{zero, one}, {one, zero}});
                decompose_CnU(Tensor<U>({{zero, one}, {one, zero}}), extract_qubit, index, ctrl_gates - 1);
            }

            V.adjoint();
            decompose_CnU(V, diff_pos, index, ctrl_gates - 1);
        }

        return 0;
    }

    template <typename U>
    int decompose_CU(Tensor<U> const& t, size_t ctrl, size_t targ) {
        const struct ZYZ angles = decompose_ZYZ(t);
        // fmt::println("angles: {}, {}, {}, {}", angles.phi, angles.alpha, angles.beta, angles.gamma);
        QubitIdList target1, target2;
        target1.emplace_back(targ);
        target2.emplace_back(ctrl);
        target2.emplace_back(targ);

        if (std::abs((angles.alpha - angles.gamma) / 2) > 1e-6) {
            _quantum_circuit->add_gate("rz", target1, dvlab::Phase{((angles.alpha - angles.gamma) / 2) * (-1.0)}, true);
        }
        if (std::abs(angles.beta) > 1e-6) {
            _quantum_circuit->add_gate("cx", target2, {}, true);
            // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
            if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6) {
                target1.emplace_back(targ);
                _quantum_circuit->add_gate("rz", target1, dvlab::Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}, true);
            }
            _quantum_circuit->add_gate("ry", target1, dvlab::Phase{angles.beta * (-1.0)}, true);
            _quantum_circuit->add_gate("cx", target2, {}, true);
            _quantum_circuit->add_gate("ry", target1, dvlab::Phase{angles.beta}, true);

            // fout << fmt::format("ry({}) q[{}];\n", angles.beta * (-1.0), targ);
            // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
            // fout << fmt::format("ry({}) q[{}];\n", angles.beta, targ);
            if (std::abs(angles.alpha) > 1e-6) {
                _quantum_circuit->add_gate("rz", target1, dvlab::Phase{angles.alpha}, true);
                // fout << fmt::format("rz({}) q[{}];\n", angles.alpha, targ);
            }
        } else {
            if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6) {
                _quantum_circuit->add_gate("cx", target2, {}, true);
                _quantum_circuit->add_gate("rz", target1, dvlab::Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}, true);
                _quantum_circuit->add_gate("cx", target2, {}, true);
                // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
                // fout << fmt::format("rz({}) q[{}];\n", ((angles.alpha + angles.gamma) / 2) * (-1.0), targ);
                // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
            }
            if (std::abs(angles.alpha) > 1e-6) {
                _quantum_circuit->add_gate("rz", target1, dvlab::Phase{angles.alpha}, true);
                // fout << fmt::format("rz({}) q[{}];\n", angles.alpha, targ);
            }
        }
        if (std::abs(angles.phi) > 1e-6) {
            QubitIdList ctrl_list;
            ctrl_list.emplace_back(ctrl);
            _quantum_circuit->add_gate("rz", ctrl_list, dvlab::Phase{angles.phi}, true);
            // fout << fmt::format("rz({}) q[{}];\n", angles.phi, ctrl);
        }

        return 0;
    }

    template <typename U>
    ZYZ decompose_ZYZ(Tensor<U> const& t) {
        // fmt::println("tensor: {}", t);
        // bool bc = 0;
        const std::complex<double> a = t(0, 0), b = t(0, 1), c = t(1, 0), d = t(1, 1);
        struct ZYZ output = {};

        // new calculation
        // fmt::println("abs: {}", std::abs(a));
        double init_beta = 0;
        if (std::abs(a) > 1) {
            init_beta = 0;
        } else {
            init_beta = std::acos(std::abs(a));
        }
        const std::array<double, 4> beta_candidate = {init_beta, PI - init_beta, PI + init_beta, 2.0 * PI - init_beta};
        for (const auto& beta : beta_candidate) {
            // fmt::println("beta: {}", beta);
            output.beta = beta;
            std::complex<double> a1, b1, c1, d1;
            const std::complex<double> cos(std::cos(beta) + 1e-5, 0), sin(std::sin(beta) + 1e-5, 0);
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
            const std::complex<double> a_r(std::cos((output.alpha + output.gamma) / 2), std::sin((output.alpha + output.gamma) / 2));
            const std::complex<double> _ar(std::cos((output.alpha - output.gamma) / 2), std::sin((output.alpha - output.gamma) / 2));
            if (std::abs(a) < 1e-4) {
                output.phi = std::arg(c1 / _ar);
            } else {
                output.phi = std::arg(a1 * a_r);
            }
            const std::complex<double> phi(std::cos(output.phi), std::sin(output.phi));

            if (std::abs(phi * cos / a_r - a) < 1e-3 && std::abs(sin * phi / _ar + b) < 1e-3 && std::abs(phi * _ar * sin - c) < 1e-3 && std::abs(phi * a_r * cos - d) < 1e-3) {
                return output;
            }
        }
        // FIXME - Error or not
        fmt::println("no solution");

        return output;
    }

    template <typename U>
    Tensor<U> sqrt_tensor(Tensor<U> const& t) {
        const std::complex<double> a = t(0, 0), b = t(0, 1), c = t(1, 0), d = t(1, 1);
        const std::complex<double> tau = a + d, delta = a * d - b * c;
        const std::complex s  = std::sqrt(delta);
        const std::complex _t = std::sqrt(tau + s + s);
        if (std::abs(_t) > 0) {
            // Tensor<U> v({{(a + s) / _t, b / _t}, {c / _t, (d + s) / _t}});
            return Tensor<U>({{(a + s) / _t, b / _t}, {c / _t, (d + s) / _t}});
            ;
        } else {
            // Tensor<U> v({{std::sqrt(a), b}, {c, std::sqrt(d)}});
            return Tensor<U>({{std::sqrt(a), b}, {c, std::sqrt(d)}});
            ;
        }
    }

private:
    QCir* _quantum_circuit;
    size_t _qreg;
};

}  // namespace decomposer

}  // namespace qsyn
