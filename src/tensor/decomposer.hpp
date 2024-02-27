/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>
#include <cstddef>


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
    TwoLevelMatrix(QTensor<T> const&_mat) : given(_mat){}

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

    Decomposer(int reg) : _quantum_circuit(new QCir(reg)), qreg(reg){
    }

    QCir* get_qcir() { return _quantum_circuit; }
    int get_qreg() { return qreg; }

    template <typename U>
    void decompose(QTensor<U> const& matrix) {

        auto mat_chain = get_2level(matrix);

        int end = mat_chain.size();

        for (int i = end - 1; i >= 0; i--) {
            int i_idx = 0, j_idx = 0;
            for (int j = 0; j < qreg; j++) {
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
            // graycode(two_level_chain[i].given, two_level_chain[i].i, two_level_chain[i].j, qreg, fs);
        }

    }

    template <typename U>
    auto get_2level(QTensor<U> t) {
        std::vector<TwoLevelMatrix<U>> two_level_chain;

        using namespace std::literals;

        int s = int(t.shape()[0]);

        QTensor<U>
            XU = QTensor<U>::identity((int)round(std::log2(s)));

        XU.reshape({t.shape()[0], t.shape()[0]});

        QTensor<U>
            FU = QTensor<U>::identity(1);

        for (int i = 0; i < s; i++) {
            for (int j = i + 1; j < s; j++) {
                // check 2-level
                bool is_two_level = false;
                int d = 0, Up = 0, L = 0;
                int d1 = 0, Ui = 0, Li = 0;
                int d2 = 0, Uj = 0, Lj = 0;
                int c_i = 0, c_j = 0;
                std::complex one(1.0, 0.);

                for (int x = 0; x < s; x++) {  // check all
                    for (int y = 0; y < s; y++) {
                        if (x == y) {
                            if (std::abs(t(y, x) - one) > 1e-6) {
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


                double u = std::sqrt(std::norm(t(i, i)) + std::norm(t(j, i)));

                using namespace std::literals;

                for (int x = 0; x < s; x++) {
                    for (int y = 0; y < s; y++) {
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
    int graycode(Tensor<U> const& t, int I, int J) {
        // fmt::println("in graycode function");
        // do pabbing
        std::vector<QubitIdList> qubits_list;
        std::vector<std::string> gate_list;
        int gates_length = 0;

        int get_diff = I ^ J;
        int diff_pos = 0;
        for (int i = 0; i < qreg; i++) {
            if (((get_diff >> i) & 1) && (J >> i & 1)) {
                diff_pos = i;
                break;
            }
        }

        bool x_given = 0;

        if ((I + std::pow(2, diff_pos)) != (std::pow(2, qreg) - 1)) {
            if (((I >> diff_pos) & 1) == 0) {
                QubitIdList target;
                target.emplace_back(diff_pos);
                qubits_list.emplace_back(target);
                gate_list.emplace_back("x");
                gates_length++;
                _quantum_circuit -> add_gate("x", target, {}, true);
                x_given = 1;
            }
            for (int i = 0; i < qreg; i++) {
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
                    _quantum_circuit -> add_gate("cx", target, {}, true);
                    // std::cout << "cx on ctrl: " << diff_pos << " targ: " << i << std::endl;
                }
            }
            if (x_given) {

                QubitIdList target;
                target.emplace_back(diff_pos);
                qubits_list.emplace_back(target);
                gate_list.emplace_back("x");
                gates_length++;
                _quantum_circuit -> add_gate("x", target, {}, true);
                
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
            _quantum_circuit -> add_gate("x", target, {}, true);  
            // std::cout << "x on " << diff_pos << std::endl;
            x_given = 1;
        }
        for (int i = 0; i < qreg; i++) {
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
                _quantum_circuit -> add_gate("cx", target, {}, true);
                    
            }
        }
        if (x_given) {
            QubitIdList target;
            target.emplace_back(diff_pos);
            qubits_list.emplace_back(target);
            gate_list.emplace_back("x");
            gates_length++;
            _quantum_circuit -> add_gate("x", target, {}, true);  
            // std::cout << "x on " << diff_pos << std::endl;
            x_given = 0;
        }
        // decompose CnU
        int ctrl_index = 0;
        for (int i = 0; i < qreg; i++) {
            if (i != diff_pos) {
                ctrl_index += int(pow(2, i));
            }
        }
        decompose_CnU(t, diff_pos, ctrl_index, qreg - 1);

        // do unpabbing
        for (int i = gates_length - 1; i >= 0; i--) {
            _quantum_circuit -> add_gate(gate_list[i], qubits_list[i], {}, true);
        }
        return diff_pos;
    }

    template <typename U>
    int decompose_CnU(Tensor<U> const& t, int diff_pos, int index, int ctrl_gates) {
        // fmt::println("in decompose to decompose CnU function ctrl_gates: {}", t);
        int ctrl = diff_pos - 1;
        if (diff_pos == 0) {
            ctrl = 1;
        }
        if (!((index >> ctrl) & 1)) {
            for (int i = 0; i < qreg; i++) {
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
            int extract_qubit = -1;
            for (int i = 0; i < qreg; i++) {
                if (i == ctrl) {
                    continue;
                }
                if ((index >> i) & 1) {
                    extract_qubit = i;
                    index         = index - int(pow(2, i));
                    break;
                }
            }
            Tensor<U> V = sqrt_tensor(t);
            // std::cout << "extract qubit: " << extract_qubit << std::endl;
            decompose_CU(V, extract_qubit, diff_pos);
            int max_index = (int(log2(index)) + 1), count = 0;
            std::vector<int> ctrls;
            for (int i = 0; i < max_index; i++) {
                if ((index >> i) & 1) {
                    ctrls.emplace_back(i);
                    count++;
                }
            }
            if (count == 1) {
                QubitIdList target;
                target.emplace_back(ctrls[0]);
                target.emplace_back(extract_qubit);
                _quantum_circuit -> add_gate("cx", target, {}, true);
            } else if (count == 2) {
                QubitIdList target;
                for (int i = 0; i < count; i++) {
                    target.emplace_back(ctrls[i]);
                }
                target.emplace_back(extract_qubit);
                _quantum_circuit -> add_gate("ccx", target, {}, true);
            } else {
                std::complex<double> zero(0, 0), one(1, 0);
                Tensor<U> x({{zero, one}, {one, zero}});
                decompose_CnU(x, extract_qubit, index, ctrl_gates - 1);
            }
            V.adjoint();
            decompose_CU(V, extract_qubit, diff_pos);
            if (count == 1) {
                QubitIdList target;
                target.emplace_back(ctrls[0]);
                target.emplace_back(extract_qubit);
                _quantum_circuit -> add_gate("cx", target, {}, true);
            } else if (count == 2) {
                QubitIdList target;
                for (int i = 0; i < count; i++) {
                    target.emplace_back(ctrls[i]);
                }
                target.emplace_back(extract_qubit);
                _quantum_circuit -> add_gate("ccx", target, {}, true);
            } else {
                std::complex<double> zero(0, 0), one(1, 0);
                Tensor<U> x({{zero, one}, {one, zero}});
                decompose_CnU(x, extract_qubit, index, ctrl_gates - 1);
            }
            
            V.adjoint();
            decompose_CnU(V, diff_pos, index, ctrl_gates - 1);
        }

        return 0;
    }

    template <typename U>
    int decompose_CU(Tensor<U> const& t, int ctrl, int targ) {
        
        struct ZYZ angles;
        
        angles = decompose_ZYZ(t);
        // fmt::println("angles: {}, {}, {}, {}", angles.phi, angles.alpha, angles.beta, angles.gamma);
        QubitIdList target1, target2;
        target1.emplace_back(targ);
        target2.emplace_back(ctrl);
        target2.emplace_back(targ);

        if (std::abs((angles.alpha - angles.gamma) / 2) > 1e-6) {
            _quantum_circuit -> add_gate("rz", target1, dvlab::Phase{((angles.alpha - angles.gamma) / 2) * (-1.0)}, true);
        }
        if (std::abs(angles.beta) > 1e-6) {
            
            _quantum_circuit -> add_gate("cx", target2, {}, true);
            // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
            if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6) {
              target1.emplace_back(targ);
              _quantum_circuit -> add_gate("rz", target1, dvlab::Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}, true);
            }
            _quantum_circuit -> add_gate("ry", target1, dvlab::Phase{angles.beta * (-1.0)}, true);
            _quantum_circuit -> add_gate("cx", target2, {}, true);
            _quantum_circuit -> add_gate("ry", target1, dvlab::Phase{angles.beta}, true);
            
            // fout << fmt::format("ry({}) q[{}];\n", angles.beta * (-1.0), targ);
            // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
            // fout << fmt::format("ry({}) q[{}];\n", angles.beta, targ);
            if (std::abs(angles.alpha) > 1e-6) {
                _quantum_circuit -> add_gate("rz", target1, dvlab::Phase{angles.alpha}, true);
                // fout << fmt::format("rz({}) q[{}];\n", angles.alpha, targ);
            }
        } else {
            if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6) {
                _quantum_circuit -> add_gate("cx", target2, {}, true);
                _quantum_circuit -> add_gate("rz", target1, dvlab::Phase{((angles.alpha + angles.gamma) / 2) * (-1.0)}, true);
                _quantum_circuit -> add_gate("cx", target2, {}, true);
                // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
                // fout << fmt::format("rz({}) q[{}];\n", ((angles.alpha + angles.gamma) / 2) * (-1.0), targ);
                // fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
            }
            if (std::abs(angles.alpha) > 1e-6) {
                _quantum_circuit -> add_gate("rz", target1, dvlab::Phase{angles.alpha}, true);
                // fout << fmt::format("rz({}) q[{}];\n", angles.alpha, targ);
            }
        }
        if (std::abs(angles.phi) > 1e-6) {
            QubitIdList ctrl_list;
            ctrl_list.emplace_back(ctrl);
            _quantum_circuit -> add_gate("rz", ctrl_list, dvlab::Phase{angles.phi}, true);
            // fout << fmt::format("rz({}) q[{}];\n", angles.phi, ctrl);
        }

        return 0;
    }

    template <typename U>
    ZYZ decompose_ZYZ(Tensor<U> const& t) {
        // fmt::println("tensor: {}", t);
        // bool bc = 0;
        std::complex<double> a = t(0, 0), b = t(0, 1), c = t(1, 0), d = t(1, 1);
        struct ZYZ output;

        // new calculation
        // fmt::println("abs: {}", std::abs(a));
        double init_beta = 0;
        if (std::abs(a) > 1) {
            init_beta = 0;
        } else {
            init_beta = std::acos(std::abs(a));
        }
        double beta_candidate[] = {init_beta, PI - init_beta, PI + init_beta, 2.0 * PI - init_beta};
        for (int i = 0; i < 4; i++) {
            double beta = beta_candidate[i];
            // fmt::println("beta: {}", beta);
            output.beta = beta;
            std::complex<double> a1, b1, c1, d1;
            std::complex<double> cos(std::cos(beta) + 1e-5, 0), sin(std::sin(beta) + 1e-5, 0);
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
            std::complex<double> a_r(std::cos((output.alpha + output.gamma) / 2), std::sin((output.alpha + output.gamma) / 2));
            std::complex<double> _ar(std::cos((output.alpha - output.gamma) / 2), std::sin((output.alpha - output.gamma) / 2));
            if (std::abs(a) < 1e-4) {
                output.phi = std::arg(c1 / _ar);
            } else {
                output.phi = std::arg(a1 * a_r);
            }
            std::complex<double> phi(std::cos(output.phi), std::sin(output.phi));

            if (std::abs(phi * cos / a_r - a) < 1e-3 && std::abs(sin * phi / _ar + b) < 1e-3 && std::abs(phi * _ar * sin - c) < 1e-3 && std::abs(phi * a_r * cos - d) < 1e-3) {
                return output;
            }
        }
        fmt::println("no solution");

        return output;
    }

    template <typename U>
    Tensor<U> sqrt_tensor(Tensor<U> const& t) {
        std::complex<double> a = t(0, 0), b = t(0, 1), c = t(1, 0), d = t(1, 1);
        std::complex<double> tau = a + d, delta = a * d - b * c;
        // std::cout << tau << " " << delta << std::endl;
        std::complex s  = std::sqrt(delta);
        std::complex _t = std::sqrt(tau + s + s);
        if (std::abs(_t) > 0) {
            Tensor<U> v({{(a + s) / _t, b / _t}, {c / _t, (d + s) / _t}});
            return v;
        } else {
            Tensor<U> v({{std::sqrt(a), b}, {c, std::sqrt(d)}});
            return v;
        }
    }

private:
    QCir* _quantum_circuit;
    int qreg;
};

}  // namespace decomposer

}  // namespace qsyn
