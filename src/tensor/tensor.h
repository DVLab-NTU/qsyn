/****************************************************************************
  FileName     [ tensor.h ]
  PackageName  [ tensor ]
  Synopsis     [ Define class Tensor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef TENSOR_BASE_CLASS_H
#define TENSOR_BASE_CLASS_H

#include <cassert>
#include <complex>
#include <concepts>
#include <exception>
#include <iosfwd>
#include <unordered_map>
#include <vector>
#include <xtensor-blas/xlinalg.hpp>
#include <xtensor/xadapt.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>

#include "tensorDef.h"
#include "tensorUtil.h"
#include "util.h"

template <typename DT>
class Tensor {
protected:
    using DataType = DT;
    using InternalType = xt::xarray<DataType>;

public:
    Tensor(xt::nested_initializer_list_t<DT, 0> il) : _tensor(il) { resetAxisHistory(); }
    Tensor(xt::nested_initializer_list_t<DT, 1> il) : _tensor(il) { resetAxisHistory(); }
    Tensor(xt::nested_initializer_list_t<DT, 2> il) : _tensor(il) { resetAxisHistory(); }
    Tensor(xt::nested_initializer_list_t<DT, 3> il) : _tensor(il) { resetAxisHistory(); }
    Tensor(xt::nested_initializer_list_t<DT, 4> il) : _tensor(il) { resetAxisHistory(); }
    Tensor(xt::nested_initializer_list_t<DT, 5> il) : _tensor(il) { resetAxisHistory(); }

    Tensor(const TensorShape& shape) : _tensor(shape) { resetAxisHistory(); }
    Tensor(TensorShape&& shape) : _tensor(shape) { resetAxisHistory(); }
    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(const From& internal) : _tensor(internal) { resetAxisHistory(); }
    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(From&& internal) : _tensor(internal) { resetAxisHistory(); }

    virtual ~Tensor() {}

    template <typename... Args>
    DT& operator()(const Args&... args);
    template <typename... Args>
    const DT& operator()(const Args&... args) const;

    DT& operator[](const TensorIndex& i);
    const DT& operator[](const TensorIndex& i) const;

    virtual bool operator==(const Tensor<DT>& rhs) const;
    virtual bool operator!=(const Tensor<DT>& rhs) const;

    template <typename U>
    friend std::ostream& operator<<(std::ostream& os, const Tensor<U>& t);  // different typename to avoid shadowing

    virtual Tensor<DT>& operator+=(const Tensor<DT>& rhs);
    virtual Tensor<DT>& operator-=(const Tensor<DT>& rhs);
    virtual Tensor<DT>& operator*=(const Tensor<DT>& rhs);
    virtual Tensor<DT>& operator/=(const Tensor<DT>& rhs);

    template <typename U>
    friend Tensor<U> operator+(Tensor<U> lhs, const Tensor<U>& rhs);
    template <typename U>
    friend Tensor<U> operator-(Tensor<U> lhs, const Tensor<U>& rhs);
    template <typename U>
    friend Tensor<U> operator*(Tensor<U> lhs, const Tensor<U>& rhs);
    template <typename U>
    friend Tensor<U> operator/(Tensor<U> lhs, const Tensor<U>& rhs);

    size_t dimension() const { return _tensor.dimension(); }
    std::vector<size_t> shape() const;

    void resetAxisHistory();
    size_t getNewAxisId(const size_t& oldId);

    template <typename U>
    friend double innerProduct(const Tensor<U>& t1, const Tensor<U>& t2);

    template <typename U>
    friend double cosineSimilarity(const Tensor<U>& t1, const Tensor<U>& t2);

    template <typename U>
    friend Tensor<U> tensordot(const Tensor<U>& t1, const Tensor<U>& t2,
                               const TensorAxisList& ax1, const TensorAxisList& ax2);

    template <typename U>
    friend Tensor<U> tensorPow(const Tensor<U>& t, size_t n);

    template <typename U>
    friend bool isPartition(const Tensor<U>& t, const TensorAxisList& axin, const TensorAxisList& axout);

    Tensor<DT> toMatrix(const TensorAxisList& axin, const TensorAxisList& axout);

    template <typename U>
    friend Tensor<U> directSum(const Tensor<U>& t1, const Tensor<U>& t2);

    void reshape(const TensorShape& shape);
    Tensor<DT> transpose(const TensorAxisList& perm) const;
    Tensor<DT> adjoint();

protected:
    InternalType _tensor;
    std::unordered_map<size_t, size_t> _axisHistory;
};

//------------------------------
// Operators
//------------------------------

template <typename DT>
template <typename... Args>
DT& Tensor<DT>::operator()(const Args&... args) {
    return _tensor(args...);
}
template <typename DT>
template <typename... Args>
const DT& Tensor<DT>::operator()(const Args&... args) const {
    return _tensor(args...);
}

template <typename DT>
DT& Tensor<DT>::operator[](const TensorIndex& i) {
    return _tensor[i];
}
template <typename DT>
const DT& Tensor<DT>::operator[](const TensorIndex& i) const {
    return _tensor[i];
}

template <typename DT>
bool Tensor<DT>::operator==(const Tensor<DT>& rhs) const {
    return _tensor == rhs._tensor;
}
template <typename DT>
bool Tensor<DT>::operator!=(const Tensor<DT>& rhs) const {
    return _tensor != rhs._tensor;
}

template <typename U>
std::ostream& operator<<(std::ostream& os, const Tensor<U>& t) {
    return os << t._tensor;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator+=(const Tensor<DT>& rhs) {
    _tensor += rhs._tensor;
    return *this;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator-=(const Tensor<DT>& rhs) {
    _tensor -= rhs._tensor;
    return *this;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator*=(const Tensor<DT>& rhs) {
    _tensor *= rhs._tensor;
    return *this;
}

template <typename DT>
Tensor<DT>& Tensor<DT>::operator/=(const Tensor<DT>& rhs) {
    _tensor /= rhs._tensor;
    return *this;
}

template <typename U>
Tensor<U> operator+(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor += rhs._tensor;
    return lhs;
}
template <typename U>
Tensor<U> operator-(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor -= rhs._tensor;
    return lhs;
}
template <typename U>
Tensor<U> operator*(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor *= rhs._tensor;
    return lhs;
}
template <typename U>
Tensor<U> operator/(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor /= rhs._tensor;
    return lhs;
}

// @brief Returns the shape of the tensor
template <typename DT>
std::vector<size_t> Tensor<DT>::shape() const {
    std::vector<size_t> shape;
    for (size_t i = 0; i < dimension(); ++i) {
        shape.push_back(_tensor.shape(i));
    }
    return shape;
}

// reset the tensor axis history to (0, 0), (1, 1), ..., (n-1, n-1)
template <typename DT>
void Tensor<DT>::resetAxisHistory() {
    _axisHistory.clear();
    for (size_t i = 0; i < _tensor.dimension(); ++i) {
        _axisHistory.emplace(i, i);
    }
}

template <typename DT>
size_t Tensor<DT>::getNewAxisId(const size_t& oldId) {
    if (!_axisHistory.contains(oldId)) {
        return (size_t)-1;
    } else {
        return _axisHistory[oldId];
    }
}

//------------------------------
// Tensor Manipulations:
// Member functions
//------------------------------

// Convert the tensor to a matrix, i.e., a 2D tensor according to the two axis lists.
template <typename DT>
Tensor<DT> Tensor<DT>::toMatrix(const TensorAxisList& axin, const TensorAxisList& axout) {
    if (!isPartition(*this, axin, axout)) {
        throw std::invalid_argument("The two axis lists should partition 0~(n-1).");
    }
    Tensor<DT> t = xt::transpose(this->_tensor, concatAxisList(axin, axout));
    t._tensor.reshape({intPow(2, axin.size()), intPow(2, axout.size())});
    return t;
}

template <typename U>
Tensor<U> directSum(const Tensor<U>& t1, const Tensor<U>& t2) {
    using shape_t = typename xt::xarray<U>::shape_type;
    if (t1.dimension() != 2 || t2.dimension() != 2) {
        throw std::invalid_argument("The two tensors should be 2-dimension.");
    }
    shape_t shape1 = t1._tensor.shape();
    shape_t shape2 = t2._tensor.shape();
    auto tmp1 = xt::hstack(xt::xtuple(t1._tensor, xt::zeros<U>({shape1[0], shape2[1]})));
    auto tmp2 = xt::hstack(xt::xtuple(xt::zeros<U>({shape2[0], shape1[1]}), t2._tensor));

    return xt::vstack(xt::xtuple(tmp1, tmp2));
}

// Rearrange the element of the tensor to a new shape
template <typename DT>
void Tensor<DT>::reshape(const TensorShape& shape) {
    this->_tensor = this->_tensor.reshape(shape);
}

// Rearrange the order of axes
template <typename DT>
Tensor<DT> Tensor<DT>::transpose(const TensorAxisList& perm) const {
    return xt::transpose(_tensor, perm);
}

template <typename DT>
Tensor<DT> Tensor<DT>::adjoint() {
    assert(dimension() == 2);
    return xt::conj(xt::transpose(_tensor, {1, 0}));
}
//------------------------------
// Tensor Manipulations:
// Friend functions
//------------------------------

// Calculate the inner products between two tensors
template <typename U>
double innerProduct(const Tensor<U>& t1, const Tensor<U>& t2) {
    if (t1.shape() != t2.shape()) {
        throw std::invalid_argument("The two tensors should have the same shape");
    }
    return xt::abs(xt::sum(xt::conj(t1._tensor) * t2._tensor))();
}
// Calculate the cosine similarity of two tensors
template <typename U>
double cosineSimilarity(const Tensor<U>& t1, const Tensor<U>& t2) {
    return innerProduct(t1, t2) / std::sqrt(innerProduct(t1, t1) * innerProduct(t2, t2));
}

// tensor-dot two tensors
// dots the two tensors along the axes in ax1 and ax2
template <typename U>
Tensor<U> tensordot(const Tensor<U>& t1, const Tensor<U>& t2,
                    const TensorAxisList& ax1 = {}, const TensorAxisList& ax2 = {}) {
    if (ax1.size() != ax2.size()) {
        throw std::invalid_argument("The two index orders should contain the same number of indices.");
    }
    Tensor<U> t = xt::linalg::tensordot(t1._tensor, t2._tensor, ax1, ax2);
    size_t tmpAxisId = 0;
    t._axisHistory.clear();
    for (size_t i = 0; i < t1._tensor.dimension(); ++i) {
        if (std::find(ax1.begin(), ax1.end(), i) != ax1.end()) continue;
        t._axisHistory.emplace(i, tmpAxisId);
        ++tmpAxisId;
    }
    for (size_t i = 0; i < t2._tensor.dimension(); ++i) {
        if (std::find(ax2.begin(), ax2.end(), i) != ax2.end()) continue;
        t._axisHistory.emplace(i + t1._tensor.dimension(), tmpAxisId);
        ++tmpAxisId;
    }
    return t;
}

// Calculate tensor powers
template <typename U>
Tensor<U> tensorPow(const Tensor<U>& t, size_t n) {
    if (n == 0) return xt::ones<U>(TensorShape(0, 2));
    if (n == 1) return t;
    auto tmp = tensorPow(t, n / 2);
    if (n % 2 == 0)
        return tensordot(tmp, tmp);
    else
        return tensordot(t, tensordot(tmp, tmp));
}

// Returns true if two axis lists form a partition spanning axis 0 to n-1, where n is the dimension of the tensor.
template <typename U>
bool isPartition(const Tensor<U>& t, const TensorAxisList& axin, const TensorAxisList& axout) {
    if (!isDisjoint(axin, axout)) return false;
    if (axin.size() + axout.size() != t._tensor.dimension()) return false;
    return true;
}

#endif  // TENSOR_BASE_CLASS_H
