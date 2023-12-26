/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define class QTensor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <gsl/narrow>

#include "./tensor.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

namespace qsyn::tensor {

template <typename T>
class QTensor : public Tensor<std::complex<T>> {
protected:
    using DataType     = typename Tensor<std::complex<T>>::DataType;
    using InternalType = typename Tensor<std::complex<T>>::InternalType;

public:
    QTensor() : Tensor<DataType>(std::complex<T>(1, 0)) {}
    QTensor(Tensor<DataType> const& t) : Tensor<DataType>(t) {}
    QTensor(Tensor<DataType>&& t) : Tensor<DataType>(std::move(t)) {}

    QTensor(xt::nested_initializer_list_t<DataType, 0> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 1> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 2> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 3> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 4> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 5> il) : Tensor<DataType>(il) {}

    ~QTensor() override = default;

    QTensor(TensorShape const& shape) : Tensor<DataType>(shape) {}
    QTensor(TensorShape&& shape) : Tensor<DataType>(std::move(shape)) {}
    template <typename From>
    requires std::convertible_to<From, InternalType>
    QTensor(From&& internal) : Tensor<DataType>(std::forward<From>(internal)) {}

    static QTensor<T> identity(size_t const& n_qubits);
    static QTensor<T> zspider(size_t const& arity, dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> xspider(size_t const& arity, dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> hbox(size_t const& arity, DataType const& a = -1.);
    static QTensor<T> xgate() {
        using namespace std::literals;
        return {{0. + 0.i, 1. + 0.i}, {1. + 0.i, 0. + 0.i}};
    }
    static QTensor<T> ygate() {
        using namespace std::literals;
        return {{0. + 0.i, 0. - 1.i}, {0. + 1.i, 0. + 0.i}};
    }
    static QTensor<T> zgate() {
        using namespace std::literals;
        return {{1. + 0.i, 0. + 0.i}, {0. + 0.i, -1. + 0.i}};
    }
    static QTensor<T> rxgate(dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> rygate(dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> rzgate(dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> pxgate(dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> pygate(dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> pzgate(dvlab::Phase const& phase = dvlab::Phase(0));
    static QTensor<T> control(QTensor<T> const& gate, size_t n_ctrls = 1);

    QTensor<T> self_tensor_dot(TensorAxisList const& ax1 = {}, TensorAxisList const& ax2 = {});

    QTensor<T> to_qtensor() const;

    template <typename U>
    friend std::complex<U> global_scalar_factor(QTensor<U> const& t1, QTensor<U> const& t2);

    template <typename U>
    friend U global_norm(QTensor<U> const& t1, QTensor<U> const& t2);

    template <typename U>
    friend dvlab::Phase global_phase(QTensor<U> const& t1, QTensor<U> const& t2);

    template <typename U>
    friend bool is_equivalent(QTensor<U> const& t1, QTensor<U> const& t2, double eps /* = 1e-6*/);

    void set_filename(std::string const& f) { _filename = f; }
    void add_procedures(std::vector<std::string> const& ps) { _procedures.insert(_procedures.end(), ps.begin(), ps.end()); }
    void add_procedure(std::string_view p) { _procedures.emplace_back(p); }

    std::string get_filename() const { return _filename; }
    std::vector<std::string> const& get_procedures() const { return _procedures; }

private:
    friend struct fmt::formatter<QTensor>;
    static DataType _nu_pow(int n);

    std::string _filename;
    std::vector<std::string> _procedures;
};

//------------------------------
// tensor builders functions
//------------------------------

/**
 * @brief Generate an tensor that corresponds to a n-qubit identity gate.
 *
 * @tparam T floating-point type
 * @param numQubits number of qubits
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::identity(size_t const& n_qubits) {
    using dvlab::utils::int_pow;
    QTensor<T> t(xt::eye<DataType>(int_pow(2, n_qubits)));
    t.reshape(TensorShape(2 * n_qubits, 2));
    TensorAxisList ax;
    for (size_t i = 0; i < n_qubits; ++i) {
        ax.emplace_back(i);
        ax.emplace_back(i + n_qubits);
    }
    return t.transpose(ax);
}

/**
 * @brief Generate an tensor that corresponds to a n-ary Z-spider.
 *
 * @tparam T
 * @param arity the dimension of the tensor
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::zspider(size_t const& arity, dvlab::Phase const& phase) {
    using namespace std::literals;
    QTensor<T> t = xt::zeros<DataType>(TensorShape(arity, 2));
    if (arity == 0) {
        t() = 1. + std::exp(1.0i * dvlab::Phase::phase_to_floating_point<T>(phase));
    } else {
        t[TensorIndex(arity, 0)] = 1.;
        t[TensorIndex(arity, 1)] = std::exp(1.0i * dvlab::Phase::phase_to_floating_point<T>(phase));
    }
    t._tensor *= _nu_pow(2 - arity);
    return t;
}

/**
 * @brief Generate an tensor that corresponds to a n-ary X-spider.
 *
 * @tparam T
 * @param arity the dimension of the tensor
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::xspider(size_t const& arity, dvlab::Phase const& phase) {
    using namespace std::literals;
    QTensor<T> t = xt::ones<QTensor<T>::DataType>(TensorShape(arity, 2));
    QTensor<T> const ket_minus({1. + 0.i, -1. + 0.i});
    QTensor<T> const tmp = tensor_product_pow(ket_minus, arity);
    t._tensor += tmp._tensor * std::exp(1.0i * dvlab::Phase::phase_to_floating_point<T>(phase));
    t._tensor /= std::pow(std::sqrt(2), arity);
    t._tensor *= _nu_pow(2 - arity);
    return t;
}

/**
 * @brief Generate an tensor that corresponds to a n-ary H-box.
 *
 * @tparam T
 * @param arity
 * @param a t(1, ..., 1) (default to -1)
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::hbox(size_t const& arity, QTensor<T>::DataType const& a) {
    QTensor<T> t = xt::ones<QTensor<T>::DataType>(TensorShape(arity, 2));
    if (arity == 0) {
        t() = a;
    } else {
        t[TensorIndex(arity, 1)] = a;
    }
    t._tensor *= _nu_pow(arity);
    return t;
}

/**
 * @brief Generate an tensor that corresponds to a Rx gate. Axis order: <in, out>
 *
 * @tparam T
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::rxgate(dvlab::Phase const& phase) {
    using namespace std::literals;
    auto t = QTensor<T>::pxgate(phase);
    t._tensor *= std::exp(-0.5i * dvlab::Phase::phase_to_floating_point<T>(phase));
    return t;
}

/**
 * @brief Generate an tensor that corresponds to a Ry gate. Axis order: <in, out>
 *
 * @tparam T
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::rygate(dvlab::Phase const& phase) {
    using namespace std::literals;
    auto t = QTensor<T>::pygate(phase);
    t._tensor *= std::exp(-0.5i * dvlab::Phase::phase_to_floating_point<T>(phase));
    return t;
}

/**
 * @brief Generate an tensor that corresponds to a Rz gate. Axis order: <in, out>
 *
 * @tparam T
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::rzgate(dvlab::Phase const& phase) {
    using namespace std::literals;
    auto t = QTensor<T>::pzgate(phase);
    t._tensor *= std::exp(-0.5i * dvlab::Phase::phase_to_floating_point<T>(phase));
    return t;
}

/**
 * @brief Generate an tensor that corresponds to a Px gate. Axis order: <in, out>
 *
 * @tparam T
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::pxgate(dvlab::Phase const& phase) {
    return QTensor<T>::xspider(2, phase);
}

/**
 * @brief Generate an tensor that corresponds to a Py gate. Axis order: <in, out>
 *
 * @tparam T
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::pygate(dvlab::Phase const& phase) {
    auto sdg = QTensor<T>::pzgate(dvlab::Phase(-1, 2));
    auto px  = QTensor<T>::pxgate(phase);
    auto s   = QTensor<T>::pzgate(dvlab::Phase(1, 2));
    return tensordot(s, tensordot(px, sdg, {1}, {0}), {1}, {0});
}

/**
 * @brief Generate an tensor that corresponds to a Pz gate. Axis order: <in, out>
 *
 * @tparam T
 * @param phase
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::pzgate(dvlab::Phase const& phase) {
    return QTensor<T>::zspider(2, phase);
}

//------------------------------
// tensor manipulation functions
//------------------------------

/**
 * @brief Tensor-dot a tensor between pairs of axes
 *
 * @tparam T
 * @param ax1 the first set of axes
 * @param ax2 the second set of axes
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::self_tensor_dot(TensorAxisList const& ax1, TensorAxisList const& ax2) {
    if (ax1.size() != ax2.size()) {
        throw std::invalid_argument("The two index orders should contain the same number of indices.");
    }
    if (!is_disjoint(ax1, ax2)) {
        throw std::invalid_argument("The two index orders should be disjoint.");
    }
    if (ax1.empty()) return *this;
    auto tmp       = QTensor<T>::identity(ax1.size());
    auto tmp_order = TensorAxisList(ax1.size() + ax2.size(), 0);
    std::iota(tmp_order.begin(), tmp_order.end(), 0);
    QTensor<T> u = tensordot(*this, tmp, concat_axis_list(ax1, ax2), tmp_order);
    u._tensor *= _nu_pow(2 * ax1.size());
    return u;
}

/**
 * @brief Generate QTensor
 *
 * @tparam T
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::to_qtensor() const {
    assert(this->dimension() == 2);
    auto dim = gsl::narrow<size_t>(std::log2(this->shape()[0]) + std::log2(this->shape()[1]));

    assert(dim % 2 == 0);

    TensorAxisList ax;
    for (size_t i = 0; i < dim / 2; ++i) {
        ax.emplace_back(i);
        ax.emplace_back(i + dim / 2);
    }
    auto result = *this;
    result.reshape(TensorAxisList(dim, 2));

    return result.transpose(ax);
}

/**
 * @brief Generate the corresponding tensor of a controlled gate.
 *
 * @tparam T
 * @param gate base gate type. e.g., rxgate, pgate, etc.
 * @param numControls number of controls
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::control(QTensor<T> const& gate, size_t n_ctrls) {
    using dvlab::utils::int_pow;
    if (n_ctrls == 0) return gate;

    auto const dim = gate.dimension();

    assert(dim % 2 == 0);

    TensorAxisList ax;

    for (size_t i = 0; i < dim / 2; ++i) {
        ax.emplace_back(2 * i);
    }
    for (size_t i = 0; i < dim / 2; ++i) {
        ax.emplace_back(2 * i + 1);
    }

    auto const gate_size     = int_pow(2, dim / 2);
    auto const identity_size = gate_size * (int_pow(2, n_ctrls) - 1);

    QTensor<T> const identity = xt::eye({identity_size, identity_size});
    QTensor<T> gate_matrix    = gate.transpose(ax);
    gate_matrix.reshape({gate_size, gate_size});

    QTensor<T> const result = direct_sum(identity, gate_matrix);
    return result.to_qtensor();
}

/**
 * @brief Get the global scalar factor between two QTensors. This function is only well defined when the cosine similarity is high between two QTensors
 *
 * @tparam U
 * @param t1 the first tensor
 * @param t2 the second tensor
 * @return std::complex<U>
 */
template <typename U>
std::complex<U> global_scalar_factor(QTensor<U> const& t1, QTensor<U> const& t2) {
    return (xt::sum(t2._tensor) / xt::sum(t1._tensor))();
}

/**
 * @brief Get the global norm between two QTensors. This function is only well defined when the cosine similarity is high between two QTensors
 *
 * @tparam U
 * @param t1 the first tensor
 * @param t2 the second tensor
 * @return U
 */
template <typename U>
U global_norm(QTensor<U> const& t1, QTensor<U> const& t2) {
    return std::abs(global_scalar_factor(t1, t2));
}

/**
 * @brief Get the global phase between two QTensors. This function is only well defined when the cosine similarity is high between two QTensors
 *
 * @tparam U
 * @param t1 the first tensor
 * @param t2 the second tensor
 * @return U
 */
template <typename U>
dvlab::Phase global_phase(QTensor<U> const& t1, QTensor<U> const& t2) {
    return dvlab::Phase(std::arg(global_scalar_factor(t1, t2)));
}

template <typename U>
bool is_equivalent(QTensor<U> const& t1, QTensor<U> const& t2, double eps = 1e-6) {
    if (t1.shape() != t2.shape()) return false;
    return cosine_similarity(t1, t2) >= (1 - eps);
}

//------------------------------
// Private member functions
//------------------------------

/**
 * @brief Calculate (2^(1/4))^n
 *
 * @tparam T
 * @param n
 * @return QTensor<T>::DataType
 */
template <typename T>
typename QTensor<T>::DataType QTensor<T>::_nu_pow(int n) {
    return std::pow(2., -0.25 * n);
}

}  // namespace qsyn::tensor

template <typename T>
struct fmt::formatter<qsyn::tensor::QTensor<T>> : fmt::ostream_formatter {};
