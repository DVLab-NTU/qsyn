/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define class Tensor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <math.h>
#include <spdlog/spdlog.h>

#include <cassert>
#include <complex>
#include <concepts>
#include <cstddef>
#include <exception>
#include <iosfwd>
#include <unordered_map>
#include <vector>
#include <xtensor-blas/xlinalg.hpp>
#include <xtensor/xadapt.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xcsv.hpp>
#include <xtensor/xio.hpp>
#include <xtensor/xnpy.hpp>

#include "./tensor_util.hpp"
#include "util/util.hpp"

#define PI 3.1415926919

namespace qsyn::tensor {

struct ZYZ {
    double phi;
    double alpha;
    double beta;  // actual beta/2
    double gamma;
};

template <typename DT>
class Tensor {
protected:
    using DataType     = DT;
    using InternalType = xt::xarray<DataType>;

public:
    // NOTE - the initialization of _tensor must use (...) because {...} is list initialization
    Tensor(xt::nested_initializer_list_t<DT, 0> il) : _tensor(il) { reset_axis_history(); }
    Tensor(xt::nested_initializer_list_t<DT, 1> il) : _tensor(il) { reset_axis_history(); }
    Tensor(xt::nested_initializer_list_t<DT, 2> il) : _tensor(il) { reset_axis_history(); }
    Tensor(xt::nested_initializer_list_t<DT, 3> il) : _tensor(il) { reset_axis_history(); }
    Tensor(xt::nested_initializer_list_t<DT, 4> il) : _tensor(il) { reset_axis_history(); }
    Tensor(xt::nested_initializer_list_t<DT, 5> il) : _tensor(il) { reset_axis_history(); }

    virtual ~Tensor() = default;

    Tensor(TensorShape const& shape) : _tensor(shape) { reset_axis_history(); }
    Tensor(TensorShape&& shape) : _tensor(std::move(shape)) { reset_axis_history(); }

    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(From const& internal) : _tensor(internal) { reset_axis_history(); }

    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(From&& internal) : _tensor(std::forward<InternalType>(internal)) { reset_axis_history(); }

    template <typename... Args>
    DT& operator()(Args const&... args);
    template <typename... Args>
    const DT& operator()(Args const&... args) const;

    DT& operator[](TensorIndex const& i);
    const DT& operator[](TensorIndex const& i) const;

    virtual bool operator==(Tensor<DT> const& rhs) const;
    virtual bool operator!=(Tensor<DT> const& rhs) const;

    template <typename U>
    friend std::ostream& operator<<(std::ostream& os, Tensor<U> const& t);  // different typename to avoid shadowing

    virtual Tensor<DT>& operator+=(Tensor<DT> const& rhs);
    virtual Tensor<DT>& operator-=(Tensor<DT> const& rhs);
    virtual Tensor<DT>& operator*=(Tensor<DT> const& rhs);
    virtual Tensor<DT>& operator/=(Tensor<DT> const& rhs);

    template <typename U>
    friend Tensor<U> operator+(Tensor<U> lhs, Tensor<U> const& rhs);
    template <typename U>
    friend Tensor<U> operator-(Tensor<U> lhs, Tensor<U> const& rhs);
    template <typename U>
    friend Tensor<U> operator*(Tensor<U> lhs, Tensor<U> const& rhs);
    template <typename U>
    friend Tensor<U> operator/(Tensor<U> lhs, Tensor<U> const& rhs);

    size_t dimension() const { return _tensor.dimension(); }
    std::vector<size_t> shape() const;

    void reset_axis_history();
    size_t get_new_axis_id(size_t const& old_id);

    template <typename U>
    friend double inner_product(Tensor<U> const& t1, Tensor<U> const& t2);

    template <typename U>
    friend double cosine_similarity(Tensor<U> const& t1, Tensor<U> const& t2);

    template <typename U>
    friend Tensor<U> tensordot(Tensor<U> const& t1, Tensor<U> const& t2,
                               TensorAxisList const& ax1, TensorAxisList const& ax2);

    template <typename U>
    friend Tensor<U> tensor_product_pow(Tensor<U> const& t, size_t n);

    template <typename U>
    friend bool is_partition(Tensor<U> const& t, TensorAxisList const& axes1, TensorAxisList const& axes2);

    Tensor<DT> to_matrix(TensorAxisList const& row_axes, TensorAxisList const& col_axis);

    template <typename U>
    friend Tensor<U> direct_sum(Tensor<U> const& t1, Tensor<U> const& t2);

    void reshape(TensorShape const& shape);
    Tensor<DT> transpose(TensorAxisList const& perm) const;
    void adjoint();

    bool tensor_read(std::string const&);
    bool tensor_write(std::string const&);

    // decomposition functions

    friend struct ZYZ;
    friend struct TwoLevelMatrix;

    template <typename U>
    friend Tensor<U> sqrt_tensor(Tensor<U> const& t);
    // template <typename U>
    // int graycode(TwoLevelMatrix const& t, int qreq);
    template <typename U>
    friend ZYZ decompose_ZYZ(Tensor<U> const& t);
    template <typename U>
    friend int decompose_CU(Tensor<U> const& t, int ctrl, int targ, std::fstream& fout);
    template <typename U>
    friend int decompose_CnU(Tensor<U> const& t, int diff_pos, int index, int ctrl_gates, int qreq, std::fstream& fout);
    template <typename U>
    friend Tensor<U> tensor_multiply(Tensor<U> const& t1, Tensor<U> const& t2);
    // void place_cx(int index, int target, std::fstream &fout);

protected:
    friend struct fmt::formatter<Tensor>;
    InternalType _tensor;
    std::unordered_map<size_t, size_t> _axis_history;
};

//------------------------------
// Operators
//------------------------------

template <typename DT>
template <typename... Args>
DT& Tensor<DT>::operator()(Args const&... args) {
    return _tensor(args...);
}
template <typename DT>
template <typename... Args>
const DT& Tensor<DT>::operator()(Args const&... args) const {
    return _tensor(args...);
}

template <typename DT>
DT& Tensor<DT>::operator[](TensorIndex const& i) {
    return _tensor[i];
}
template <typename DT>
const DT& Tensor<DT>::operator[](TensorIndex const& i) const {
    return _tensor[i];
}

template <typename DT>
bool Tensor<DT>::operator==(Tensor<DT> const& rhs) const {
    return _tensor == rhs._tensor;
}
template <typename DT>
bool Tensor<DT>::operator!=(Tensor<DT> const& rhs) const {
    return _tensor != rhs._tensor;
}

template <typename U>
std::ostream& operator<<(std::ostream& os, Tensor<U> const& t) {
    return os << t._tensor;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator+=(Tensor<DT> const& rhs) {
    _tensor += rhs._tensor;
    return *this;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator-=(Tensor<DT> const& rhs) {
    _tensor -= rhs._tensor;
    return *this;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator*=(Tensor<DT> const& rhs) {
    _tensor *= rhs._tensor;
    return *this;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator/=(Tensor<DT> const& rhs) {
    _tensor /= rhs._tensor;
    return *this;
}

template <typename U>
Tensor<U> operator+(Tensor<U> lhs, Tensor<U> const& rhs) {
    lhs._tensor += rhs._tensor;
    return lhs;
}
template <typename U>
Tensor<U> operator-(Tensor<U> lhs, Tensor<U> const& rhs) {
    lhs._tensor -= rhs._tensor;
    return lhs;
}
template <typename U>
Tensor<U> operator*(Tensor<U> lhs, Tensor<U> const& rhs) {
    lhs._tensor *= rhs._tensor;
    return lhs;
}
template <typename U>
Tensor<U> operator/(Tensor<U> lhs, Tensor<U> const& rhs) {
    lhs._tensor /= rhs._tensor;
    return lhs;
}

// @brief Returns the shape of the tensor
template <typename DT>
std::vector<size_t> Tensor<DT>::shape() const {
    std::vector<size_t> shape;
    for (size_t i = 0; i < dimension(); ++i) {
        shape.emplace_back(_tensor.shape(i));
    }
    return shape;
}

// reset the tensor axis history to (0, 0), (1, 1), ..., (n-1, n-1)
template <typename DT>
void Tensor<DT>::reset_axis_history() {
    _axis_history.clear();
    for (size_t i = 0; i < _tensor.dimension(); ++i) {
        _axis_history.emplace(i, i);
    }
}

template <typename DT>
size_t Tensor<DT>::get_new_axis_id(size_t const& old_id) {
    if (!_axis_history.contains(old_id)) {
        return SIZE_MAX;
    } else {
        return _axis_history[old_id];
    }
}

//------------------------------
// Tensor Manipulations:
// Member functions
//------------------------------

// Convert the tensor to a matrix, i.e., a 2D tensor according to the two axis lists.
template <typename DT>
Tensor<DT> Tensor<DT>::to_matrix(TensorAxisList const& row_axes, TensorAxisList const& col_axes) {
    using dvlab::utils::int_pow;
    if (!is_partition(*this, row_axes, col_axes)) {
        throw std::invalid_argument("The two axis lists should partition 0~(n-1).");
    }
    Tensor<DT> t = xt::transpose(this->_tensor, concat_axis_list(row_axes, col_axes));
    t._tensor.reshape({int_pow(2, row_axes.size()), int_pow(2, col_axes.size())});
    return t;
}

template <typename U>
Tensor<U> direct_sum(Tensor<U> const& t1, Tensor<U> const& t2) {
    using shape_t = typename xt::xarray<U>::shape_type;
    if (t1.dimension() != 2 || t2.dimension() != 2) {
        throw std::invalid_argument("The two tensors should be 2-dimension.");
    }
    shape_t shape1 = t1._tensor.shape();
    shape_t shape2 = t2._tensor.shape();
    auto tmp1      = xt::hstack(xt::xtuple(t1._tensor, xt::zeros<U>({shape1[0], shape2[1]})));
    auto tmp2      = xt::hstack(xt::xtuple(xt::zeros<U>({shape2[0], shape1[1]}), t2._tensor));

    return xt::vstack(xt::xtuple(tmp1, tmp2));
}

// Rearrange the element of the tensor to a new shape
template <typename DT>
void Tensor<DT>::reshape(TensorShape const& shape) {
    this->_tensor = this->_tensor.reshape(shape);
}

// Rearrange the order of axes
template <typename DT>
Tensor<DT> Tensor<DT>::transpose(TensorAxisList const& perm) const {
    return xt::transpose(_tensor, perm);
}

template <typename DT>
void Tensor<DT>::adjoint() {
    assert(dimension() == 2);
    _tensor = xt::conj(xt::transpose(_tensor, {1, 0}));
}

template <typename DT>
bool Tensor<DT>::tensor_write(std::string const& filepath) {
    std::ofstream out_file;
    out_file.open(filepath);
    if (!out_file.is_open()) {
        spdlog::error("Failed to open file");
        return false;
    }
    for (size_t row = 0; row < _tensor.shape(0); row++) {
        for (size_t col = 0; col < _tensor.shape(1); col++) {
            if (xt::imag(this->_tensor(row, col)) >= 0) {
                fmt::print(out_file, "{}{}+{}j", xt::real(_tensor(row, col)) >= 0 ? " " : "", xt::real(_tensor(row, col)), abs(xt::imag(_tensor(row, col))));
            } else {
                fmt::print(out_file, "{}{}{}j", xt::real(_tensor(row, col)) >= 0 ? " " : "", xt::real(_tensor(row, col)), xt::imag(_tensor(row, col)));
            }
            if (col != _tensor.shape(1) - 1) {
                fmt::print(out_file, ", ");
            }
        }
        fmt::println(out_file, "");
    }
    out_file.close();
    return true;
}

template <typename DT>
bool Tensor<DT>::tensor_read(std::string const& filepath) {
    std::ifstream in_file;
    in_file.open(filepath);
    if (!in_file.is_open()) {
        spdlog::error("Failed to open file");
        return false;
    }
    // read csv with complex number
    std::vector<std::complex<double>> data;
    std::string line, word;
    while (std::getline(in_file, line)) {
        std::stringstream ss(line);
        while (std::getline(ss, word, ',')) {
            std::stringstream ww(word);

            double real = 0.0, imag = 0.0;
            char plus = ' ', i = ' ';
            ww >> real >> plus >> imag >> i;
            if (plus == '-') imag = -imag;
            if (plus == 'j') {
                imag = real;
                real = 0;
            }
            data.push_back({real, imag});
        }
    }
    if (std::floor(std::sqrt(data.size())) != std::sqrt(data.size())) {
        spdlog::error("The number of elements in the tensor is not a square number");
        return false;
    }
    TensorShape shape = {static_cast<size_t>(std::sqrt(data.size())), static_cast<size_t>(std::sqrt(data.size()))};
    this->_tensor     = xt::adapt(data, shape);
    return true;
}

//------------------------------
// Tensor Manipulations:
// Friend functions
//------------------------------

// Calculate the inner products between two tensors
template <typename U>
double inner_product(Tensor<U> const& t1, Tensor<U> const& t2) {
    if (t1.shape() != t2.shape()) {
        throw std::invalid_argument("The two tensors should have the same shape");
    }
    return xt::abs(xt::sum(xt::conj(t1._tensor) * t2._tensor))();
}
// Calculate the cosine similarity of two tensors
template <typename U>
double cosine_similarity(Tensor<U> const& t1, Tensor<U> const& t2) {
    return inner_product(t1, t2) / std::sqrt(inner_product(t1, t1) * inner_product(t2, t2));
}

// tensor-dot two tensors
// dots the two tensors along the axes in ax1 and ax2
template <typename U>
Tensor<U> tensordot(Tensor<U> const& t1, Tensor<U> const& t2,
                    TensorAxisList const& ax1 = {}, TensorAxisList const& ax2 = {}) {
    if (ax1.size() != ax2.size()) {
        throw std::invalid_argument("The two index orders should contain the same number of indices.");
    }
    Tensor<U> t        = xt::linalg::tensordot(t1._tensor, t2._tensor, ax1, ax2);
    size_t tmp_axis_id = 0;
    t._axis_history.clear();
    for (size_t i = 0; i < t1._tensor.dimension(); ++i) {
        if (std::find(ax1.begin(), ax1.end(), i) != ax1.end()) continue;
        t._axis_history.emplace(i, tmp_axis_id);
        ++tmp_axis_id;
    }
    for (size_t i = 0; i < t2._tensor.dimension(); ++i) {
        if (std::find(ax2.begin(), ax2.end(), i) != ax2.end()) continue;
        t._axis_history.emplace(i + t1._tensor.dimension(), tmp_axis_id);
        ++tmp_axis_id;
    }
    return t;
}

// Calculate tensor powers
template <typename U>
Tensor<U> tensor_product_pow(Tensor<U> const& t, size_t n) {
    if (n == 0) return xt::ones<U>(TensorShape(0, 2));
    if (n == 1) return t;
    auto tmp = tensor_product_pow(t, n / 2);
    if (n % 2 == 0)
        return tensordot(tmp, tmp);
    else
        return tensordot(t, tensordot(tmp, tmp));
}

// Returns true if two axis lists form a partition spanning axis 0 to n-1, where n is the dimension of the tensor.
template <typename U>
bool is_partition(Tensor<U> const& t, TensorAxisList const& axes1, TensorAxisList const& axes2) {
    if (!is_disjoint(axes1, axes2)) return false;
    if (axes1.size() + axes2.size() != t._tensor.dimension()) return false;
    return true;
}

//------------------------------
// DECOMPOSITION FUNCTIONS:
//
//------------------------------

// Two Level Matrices

struct TwoLevelMatrix {
    Tensor<std::complex<double>> given;
    size_t i, j;  // i < j
    TwoLevelMatrix(Tensor<std::complex<double>> U) : given(U) {}
};

// 2*2matrix sqrt
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

template <typename U>
ZYZ decompose_ZYZ(Tensor<U> const& t) {
    // fmt::println("tensor: {}", t);
    // bool bc = 0;
    std::complex<double> a = t(0, 0), b = t(0, 1), c = t(1, 0), d = t(1, 1);
    struct ZYZ output;

    // new calculation
    // fmt::println("abs: {}", std::abs(a));
    double init_beta;
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
int decompose_CU(Tensor<U> const& t, int ctrl, int targ, std::fstream& fout) {
    // fmt::println("in decompose CU function");
    // std::cout << "CU on ctrl: " << ctrl << " targ: " << targ << " sqrt_time: " << sqrt_time << " dag: " << dag << " other: " << t.shape()[0] << std::endl;
    // fmt::println("matrix: {}", t);
    struct ZYZ angles;
    // if(std::abs(t(0,0)) < 1e-6){
    //     fmt::println("cx q[{}], q[{}];"-ctrl-targ);
    //     if(std::abs(t(0,1).real()-1.0) < 1e-6){
    //         return 0;
    //     }
    //     // t(0,0).real() = t(0,1).real();
    //     // t(1,1).real() = t(1,0).real();
    //     // t(0,0).imag() = t(0,1).imag();
    //     // t(1,1).imag() = t(1,0).imag();
    //     // t(0,1).real() = 0.0;
    //     // t(0,1).imag() = 0.0;
    //     // t(1,0).real() = 0.0;
    //     // t(1,0).imag() = 0.0;
    //     Tensor<U> other = {{t(0,1), t(0,0)},{t(1,1), t(1,0)}};
    //     angles = decompose_ZYZ(other);
    // }
    // else{
    //     angles = decompose_ZYZ(t);
    // }
    angles = decompose_ZYZ(t);
    // fmt::println("angles: {}, {}, {}, {}", angles.phi, angles.alpha, angles.beta, angles.gamma);

    if (std::abs((angles.alpha - angles.gamma) / 2) > 1e-6) {
        fout << fmt::format("rz({}) q[{}];\n", ((angles.alpha - angles.gamma) / 2) * (-1.0), targ);
    }
    if (std::abs(angles.beta) > 1e-6) {
        fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
        if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6) {
            fout << fmt::format("rz({}) q[{}];\n", ((angles.alpha + angles.gamma) / 2) * (-1.0), targ);
        }
        fout << fmt::format("ry({}) q[{}];\n", angles.beta * (-1.0), targ);
        fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
        fout << fmt::format("ry({}) q[{}];\n", angles.beta, targ);
        if (std::abs(angles.alpha) > 1e-6) {
            fout << fmt::format("rz({}) q[{}];\n", angles.alpha, targ);
        }
    } else {
        if (std::abs((angles.alpha + angles.gamma) / 2) > 1e-6) {
            fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
            fout << fmt::format("rz({}) q[{}];\n", ((angles.alpha + angles.gamma) / 2) * (-1.0), targ);
            fout << fmt::format("cx q[{}], q[{}];\n", ctrl, targ);
        }
        if (std::abs(angles.alpha) > 1e-6) {
            fout << fmt::format("rz({}) q[{}];\n", angles.alpha, targ);
        }
    }
    if (std::abs(angles.phi) > 1e-6) {
        fout << fmt::format("rz({}) q[{}];\n", angles.phi, ctrl);
    }

    return 0;
}

template <typename U>
int decompose_CnU(Tensor<U> const& t, int diff_pos, int index, int ctrl_gates, int qreq, std::fstream& fout) {
    // fmt::println("in decompose to decompose CnU function ctrl_gates: {}", t);
    int ctrl = diff_pos - 1;
    if (diff_pos == 0) {
        ctrl = 1;
    }
    if (!((index >> ctrl) & 1)) {
        for (int i = 0; i < qreq; i++) {
            if ((i == diff_pos) || (i == ctrl)) {
                continue;
            } else if ((index >> i) & 1) {
                ctrl = i;
                break;
            }
        }
    }
    if (ctrl_gates == 1) {
        decompose_CU(t, ctrl, diff_pos, fout);
    } else {
        int extract_qubit = -1;
        for (int i = 0; i < qreq; i++) {
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
        decompose_CU(V, extract_qubit, diff_pos, fout);
        int max_index = (int(log2(index)) + 1), count = 0;
        std::vector<int> ctrls;
        for (int i = 0; i < max_index; i++) {
            if ((index >> i) & 1) {
                ctrls.emplace_back(i);
                count++;
            }
        }
        if (count == 1) {
            fout << fmt::format("cx q[{}], q[{}];\n", ctrls[0], extract_qubit);
        } else if (count == 2) {
            fout << "ccx ";
            for (int i = 0; i < count; i++) {
                fout << fmt::format("q[{}], ", ctrls[i]);
            }

            fout << fmt::format("q[{}];\n", extract_qubit);
        } else {
            std::complex<double> zero(0, 0), one(1, 0);
            Tensor<U> x({{zero, one}, {one, zero}});
            decompose_CnU(x, extract_qubit, index, ctrl_gates - 1, qreq, fout);
        }
        // place_cx(index, extract_qubit, fout);
        // fout << fmt::format("cx q[{}], q[{}];\n", int(log2(index)), extract_qubit);
        // std::cout << "cx " << index << " targ: " << extract_qubit << std::endl;
        V.adjoint();
        decompose_CU(V, extract_qubit, diff_pos, fout);
        if (count == 1) {
            fout << fmt::format("cx q[{}], q[{}];\n", ctrls[0], extract_qubit);
        } else if (count == 2) {
            fout << "ccx ";
            for (int i = 0; i < count; i++) {
                fout << fmt::format("q[{}], ", ctrls[i]);
            }

            fout << fmt::format("q[{}];\n", extract_qubit);
        } else {
            std::complex<double> zero(0, 0), one(1, 0);
            Tensor<U> x({{zero, one}, {one, zero}});
            decompose_CnU(x, extract_qubit, index, ctrl_gates - 1, qreq, fout);
        }
        // place_cx(index, extract_qubit, fout);
        // fout << fmt::format("cx q[{}], q[{}];\n", int(log2(index)), extract_qubit);
        // std::cout << "cx " << index << " targ: " << extract_qubit << std::endl;
        V.adjoint();
        decompose_CnU(V, diff_pos, index, ctrl_gates - 1, qreq, fout);
    }
    // std::cout << "ctrl: " << ctrl << std::endl;
    return 0;
}

template <typename U>
Tensor<U> tensor_multiply(Tensor<U> const& t1, Tensor<U> const& t2) {
    // fmt::println("in tensor multiply function");
    return tensordot(t1, t2, {1}, {0});
}

}  // namespace qsyn::tensor

template <typename DT>
struct fmt::formatter<qsyn::tensor::Tensor<DT>> : fmt::ostream_formatter {};
