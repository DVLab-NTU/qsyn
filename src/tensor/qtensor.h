/****************************************************************************
  FileName     [ qtensor.h ]
  PackageName  [ tensor ]
  Synopsis     [ Define class QTensor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef Q_TENSOR_H
#define Q_TENSOR_H

#include "phase.h"
#include "tensor.h"

template <typename T>
class QTensor : public Tensor<std::complex<T>> {
protected:
    using DataType = typename Tensor<std::complex<T>>::DataType;
    using InternalType = typename Tensor<std::complex<T>>::InternalType;

public:
    QTensor() : Tensor<DataType>(std::complex<T>(1, 0)) {}
    QTensor(const Tensor<DataType>& t) : Tensor<DataType>(t) {}
    QTensor(Tensor<DataType>&& t) : Tensor<DataType>(t) {}

    QTensor(xt::nested_initializer_list_t<DataType, 0> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 1> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 2> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 3> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 4> il) : Tensor<DataType>(il) {}
    QTensor(xt::nested_initializer_list_t<DataType, 5> il) : Tensor<DataType>(il) {}

    QTensor(const TensorShape& shape) : Tensor<DataType>(shape) {}
    QTensor(TensorShape&& shape) : Tensor<DataType>(shape) {}
    template <typename From>
    requires std::convertible_to<From, InternalType>
    QTensor(const From& internal) : Tensor<DataType>(internal) {}
    template <typename From>
    requires std::convertible_to<From, InternalType>
    QTensor(From&& internal) : Tensor<DataType>(internal) {}

    virtual ~QTensor() {}

    static QTensor<T> identity(const size_t& numQubits);
    static QTensor<T> zspider(const size_t& arity, const Phase& phase = Phase(0));
    static QTensor<T> xspider(const size_t& arity, const Phase& phase = Phase(0));
    static QTensor<T> hbox(const size_t& arity, const DataType& a = -1.);
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
    static QTensor<T> rxgate(const Phase& phase = Phase(0));
    static QTensor<T> rygate(const Phase& phase = Phase(0));
    static QTensor<T> rzgate(const Phase& phase = Phase(0));
    static QTensor<T> pxgate(const Phase& phase = Phase(0));
    static QTensor<T> pygate(const Phase& phase = Phase(0));
    static QTensor<T> pzgate(const Phase& phase = Phase(0));
    static QTensor<T> control(const QTensor<T>& gate, size_t numControls = 1);

    QTensor<T> selfTensordot(const TensorAxisList& ax1 = {}, const TensorAxisList& ax2 = {});

    QTensor<T> toQTensor();

    template <typename U>
    friend std::complex<U> globalScalarFactor(const QTensor<U>& t1, const QTensor<U>& t2);

    template <typename U>
    U globalNorm(const QTensor<U>& t1, const QTensor<U>& t2);

    template <typename U>
    Phase globalPhase(const QTensor<U>& t1, const QTensor<U>& t2);

private:
    static DataType nuPow(const int& n);
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
QTensor<T> QTensor<T>::identity(const size_t& numQubits) {
    QTensor<T> t(xt::eye<DataType>(intPow(2, numQubits)));
    t.reshape(TensorShape(2 * numQubits, 2));
    TensorAxisList ax;
    for (size_t i = 0; i < numQubits; ++i) {
        ax.push_back(i);
        ax.push_back(i + numQubits);
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
QTensor<T> QTensor<T>::zspider(const size_t& arity, const Phase& phase) {
    using namespace std::literals;
    QTensor<T> t = xt::zeros<DataType>(TensorShape(arity, 2));
    if (arity == 0) {
        t() = 1. + std::exp(1.0i * phase.toFloatType<T>());
    } else {
        t[TensorIndex(arity, 0)] = 1.;
        t[TensorIndex(arity, 1)] = std::exp(1.0i * phase.toFloatType<T>());
    }
    t._tensor *= nuPow(2 - arity);
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
QTensor<T> QTensor<T>::xspider(const size_t& arity, const Phase& phase) {
    using namespace std::literals;
    QTensor<T> t = xt::ones<QTensor<T>::DataType>(TensorShape(arity, 2));
    QTensor<T> ketMinus(TensorShape{2});
    ketMinus(0, 0) = 1.;
    ketMinus(0, 1) = -1.;
    QTensor<T> tmp = tensorPow(ketMinus, arity);
    t._tensor += tmp._tensor * std::exp(1.0i * phase.toFloatType<T>());
    t._tensor /= std::pow(std::sqrt(2), arity);
    t._tensor *= nuPow(2 - arity);
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
QTensor<T> QTensor<T>::hbox(const size_t& arity, const QTensor<T>::DataType& a) {
    QTensor<T> t = xt::ones<QTensor<T>::DataType>(TensorShape(arity, 2));
    if (arity == 0) {
        t() = a;
    } else {
        t[TensorIndex(arity, 1)] = a;
    }
    t._tensor *= nuPow(arity);
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
QTensor<T> QTensor<T>::rxgate(const Phase& phase) {
    using namespace std::literals;
    auto t = QTensor<T>::pxgate(phase);
    t._tensor *= std::exp(-0.5i * phase.toFloatType<T>());
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
QTensor<T> QTensor<T>::rygate(const Phase& phase) {
    using namespace std::literals;
    auto t = QTensor<T>::pygate(phase);
    t._tensor *= std::exp(-0.5i * phase.toFloatType<T>());
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
QTensor<T> QTensor<T>::rzgate(const Phase& phase) {
    using namespace std::literals;
    auto t = QTensor<T>::pzgate(phase);
    t._tensor *= std::exp(-0.5i * phase.toFloatType<T>());
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
QTensor<T> QTensor<T>::pxgate(const Phase& phase) {
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
QTensor<T> QTensor<T>::pygate(const Phase& phase) {
    auto sdg = QTensor<T>::pzgate(Phase(-1, 2));
    auto px = QTensor<T>::pxgate(phase);
    auto s = QTensor<T>::pzgate(Phase(1, 2));
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
QTensor<T> QTensor<T>::pzgate(const Phase& phase) {
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
QTensor<T> QTensor<T>::selfTensordot(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    if (ax1.size() != ax2.size()) {
        throw std::invalid_argument("The two index orders should contain the same number of indices.");
    }
    if (!isDisjoint(ax1, ax2)) {
        throw std::invalid_argument("The two index orders should be disjoint.");
    }
    if (ax1.empty()) return *this;
    auto tmp = QTensor<T>::identity(ax1.size());
    auto tmpOrder = TensorAxisList(ax1.size() + ax2.size(), 0);
    std::iota(tmpOrder.begin(), tmpOrder.end(), 0);
    QTensor<T> u = tensordot(*this, tmp, concatAxisList(ax1, ax2), tmpOrder);
    u._tensor *= nuPow(2 * ax1.size());
    return u;
}

/**
 * @brief Generate QTensor
 *
 * @tparam T
 * @return QTensor<T>
 */
template <typename T>
QTensor<T> QTensor<T>::toQTensor() {
    assert(this->dimension() == 2);
    size_t dim = std::log2(this->shape()[0]) + std::log2(this->shape()[1]);

    assert(dim % 2 == 0);

    TensorAxisList ax;
    for (size_t i = 0; i < dim / 2; ++i) {
        ax.push_back(i);
        ax.push_back(i + dim / 2);
    }
    this->reshape(TensorAxisList(dim, 2));

    return this->transpose(ax);
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
QTensor<T> QTensor<T>::control(const QTensor<T>& gate, size_t numControls) {
    if (numControls == 0) return gate;

    size_t dim = gate.dimension();

    assert(dim % 2 == 0);

    TensorAxisList ax;

    for (size_t i = 0; i < dim / 2; ++i) {
        ax.push_back(2 * i);
    }
    for (size_t i = 0; i < dim / 2; ++i) {
        ax.push_back(2 * i + 1);
    }

    size_t gateSize = intPow(2, dim / 2);
    size_t identitySize = gateSize * (intPow(2, numControls) - 1);

    QTensor<T> identity = xt::eye({identitySize, identitySize});
    QTensor<T> gateMatrix = gate.transpose(ax);
    gateMatrix.reshape({gateSize, gateSize});

    QTensor<T> result = directSum(identity, gateMatrix);
    return result.toQTensor();
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
std::complex<U> globalScalarFactor(const QTensor<U>& t1, const QTensor<U>& t2) {
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
U globalNorm(const QTensor<U>& t1, const QTensor<U>& t2) {
    return std::abs(globalScalarFactor(t1, t2));
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
Phase globalPhase(const QTensor<U>& t1, const QTensor<U>& t2) {
    return Phase(std::arg(globalScalarFactor(t1, t2)));
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
typename QTensor<T>::DataType QTensor<T>::nuPow(const int& n) {
    return std::pow(2., -0.25 * n);
}

#endif  // Q_TENSOR_H