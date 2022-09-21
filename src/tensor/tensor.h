/****************************************************************************
  FileName     [ tensor.h ]
  PackageName  [ tensor ]
  Synopsis     [ Definition of the Tensor class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 9 ]
****************************************************************************/
#ifndef I_TENSOR_BASE_CLASS_H
#define I_TENSOR_BASE_CLASS_H

#include <algorithm>
#include <cassert>
#include <complex>
#include <concepts>
#include <exception>
#include <iostream>
#include <numeric>
#include <vector>
#include <xtensor-blas/xlinalg.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xadapt.hpp>
#include <xtensor/xio.hpp>

#include "tensorDef.h"
#include "util.h" 
#include "tensorUtil.h"

template <typename DT>
class Tensor {
protected:
    using DataType = DT;
    using InternalType = xt::xarray<DataType>;

    
public:
    Tensor(xt::nested_initializer_list_t<DT, 0> il): _tensor(il) {}
    Tensor(xt::nested_initializer_list_t<DT, 1> il): _tensor(il) {}
    Tensor(xt::nested_initializer_list_t<DT, 2> il): _tensor(il) {}
    Tensor(xt::nested_initializer_list_t<DT, 3> il): _tensor(il) {}
    Tensor(xt::nested_initializer_list_t<DT, 4> il): _tensor(il) {}
    Tensor(xt::nested_initializer_list_t<DT, 5> il): _tensor(il) {}

    Tensor(const TensorShape& shape) : _tensor(shape) {}
    Tensor(TensorShape&& shape) : _tensor(shape) {}
    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(const From& internal) : _tensor(internal) {}
    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(From&& internal) : _tensor(internal) {}

    template <typename... Args>
    DT& operator()(const Args&... args);
    template <typename... Args>
    const DT& operator()(const Args&... args) const;

    DT& operator[](const TensorIndex& i);
    const DT& operator[](const TensorIndex& i) const;

    virtual bool operator== (const Tensor<DT>& rhs) const;
    virtual bool operator!= (const Tensor<DT>& rhs) const;

    template <typename U>
    friend std::ostream& operator<<(std::ostream& os, const Tensor<U>& t); // different typename to avoid shadowing

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
    
    template <typename U>
    friend Tensor<U> tensordot(const Tensor<U>& t1, const Tensor<U>& t2,
                                const TensorAxisList& ax1, const TensorAxisList& ax2);

    template<typename U>
    friend bool isPartition(const Tensor<U>& t, const TensorAxisList& axin, const TensorAxisList& axout);

    Tensor<DT> toMatrix(const TensorAxisList& axin, const TensorAxisList& axout);

    void reshape(const TensorShape& shape);
    Tensor<DT> transpose(const TensorAxisList& perm);
protected:
    InternalType _tensor;

    // static TensorAxisList concatAxisList(const TensorAxisList& ax1, const TensorAxisList& ax2);
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


template<typename DT>
DT& Tensor<DT>::operator[](const TensorIndex& i) {
    return _tensor[i];
}
template<typename DT>
const DT& Tensor<DT>::operator[](const TensorIndex& i) const {
    return _tensor[i];
}

template<typename DT>
bool Tensor<DT>::operator== (const Tensor<DT>& rhs) const {
    return _tensor == rhs._tensor;
}
template<typename DT>
bool Tensor<DT>::operator!= (const Tensor<DT>& rhs) const {
    return _tensor != rhs._tensor;
}

template <typename U>
std::ostream& operator<<(std::ostream& os, const Tensor<U>& t) {
    return os << t._tensor;
}

template<typename DT>
Tensor<DT>& Tensor<DT>::operator+=(const Tensor<DT>& rhs) {
    _tensor += rhs._tensor;
    return *this;
}

template<typename DT>
Tensor<DT>& Tensor<DT>::operator-=(const Tensor<DT>& rhs) {
    _tensor -= rhs._tensor;
    return *this;
}

template<typename DT>
Tensor<DT>& Tensor<DT>::operator*=(const Tensor<DT>& rhs) {
    _tensor *= rhs._tensor;
    return *this;
}

template<typename DT>
Tensor<DT>& Tensor<DT>::operator/=(const Tensor<DT>& rhs) {
    _tensor /= rhs._tensor;
    return *this;
}

template<typename U>
Tensor<U> operator+(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor += rhs._tensor;
    return lhs;
}
template<typename U>
Tensor<U> operator-(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor -= rhs._tensor;
    return lhs;
}
template<typename U>
Tensor<U> operator*(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor *= rhs._tensor;
    return lhs;
}
template<typename U>
Tensor<U> operator/(Tensor<U> lhs, const Tensor<U>& rhs) {
    lhs._tensor /= rhs._tensor;
    return lhs;
}

//------------------------------
// Tensor manipulation functions
//------------------------------

// Convert the tensor to a matrix, i.e., a 2D tensor according to the two axis lists.
template<typename DT>
Tensor<DT> Tensor<DT>::toMatrix(const TensorAxisList& axin, const TensorAxisList& axout) {
    if (!isPartition(*this, axin, axout)) {
        throw std::invalid_argument("The two axis lists should partition 0~(n-1).");
    }
    Tensor<DT> t = xt::transpose(this->_tensor, concatAxisList(axin, axout));
    t._tensor.reshape({intPow(2, axin.size()), intPow(2, axout.size())});
    return t;
}

// Rearrange the element of the tensor to a new shape
template<typename DT>
void Tensor<DT>::reshape(const TensorShape& shape) { 
    this->_tensor = this->_tensor.reshape(shape);
}

// tensor-dot two tensors
// dots the two tensors along the axes in ax1 and ax2
template<typename U>
Tensor<U> tensordot(const Tensor<U>& t1, const Tensor<U>& t2,
                            const TensorAxisList& ax1 = {}, const TensorAxisList& ax2 = {}) {
    if (ax1.size() != ax2.size()) {
        throw std::invalid_argument("The two index orders should contain the same number of indices.");
    }
    Tensor<U> t = xt::linalg::tensordot(t1._tensor, t2._tensor, ax1, ax2);
    return t;
}

// Calculate tensor powers
template<typename U>
Tensor<U> tensorPow(const Tensor<U>& t, size_t n) {
    if (n == 0) return xt::ones<U>(TensorShape(0, 2));
    if (n == 1) return t;
    auto tmp = tensorPow(t, n / 2);
    if (n % 2 == 0)
        return tensordot(tmp, tmp);
    else 
        return tensordot(t, tensordot(tmp, tmp));
}

// Rearrange the order of axes
template<typename DT>
Tensor<DT> Tensor<DT>::transpose(const TensorAxisList& perm) {
    return xt::transpose(_tensor, perm);
}

// Returns true if two axis lists form a partition spanning axis 0 to n-1, where n is the dimension of the tensor.
template<typename U>
bool isPartition(const Tensor<U>& t, const TensorAxisList& axin, const TensorAxisList& axout) {
    if (!isDisjoint(axin, axout)) return false;
    if (axin.size() + axout.size() != t._tensor.dimension()) return false;
    return true;
}


#endif //I_TENSOR_BASE_CLASS_H
