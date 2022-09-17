/****************************************************************************
  FileName     [ tensor.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Tensor class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 9 ]
****************************************************************************/
#ifndef TENSOR_H
#define TENSOR_H

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

#include "phase.h"
#include "util.h"

using TensorShape = std::vector<size_t>;
using TensorIndex = std::vector<size_t>;
using TensorAxisList = std::vector<size_t>;

template <typename T>
class Tensor {
    using DataType = std::complex<T>;
    using InternalType = xt::xarray<DataType>;
    

public:
    Tensor(const TensorShape& shape) : _tensor(shape) {}
    Tensor(TensorShape&& shape) : _tensor(shape) {}
    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(const From& internal) : _tensor(internal) {}
    template <typename From>
    requires std::convertible_to<From, InternalType>
    Tensor(From&& internal) : _tensor(internal) {}

    friend std::ostream& operator<<(std::ostream& os, const Tensor<T>& t) {
        return os << t._tensor;
    }

    template <typename... Args>
    DataType& operator()(const Args&... args) {
        return _tensor(args...);
    }
    template <typename... Args>
    const DataType& operator()(const Args&... args) const {
        return _tensor(args...);
    }

    DataType& operator[](const TensorIndex& i) {
        return _tensor[i];
    }
    const DataType& operator[](const TensorIndex& i) const {
        return _tensor[i];
    }

    bool operator== (const Tensor<T>& rhs) const {
        return _tensor == rhs._tensor;
    }

    bool operator!= (const Tensor<T>& rhs) const {
        return _tensor != rhs._tensor;
    }

    // Generate an tensor that corresponds to a n-qubit identity gate.
    // Note that `n` is the number of qubits, not dimensions
    static Tensor<T> identity(const size_t& n) {
        InternalType t = xt::eye<DataType>(intPow(2, n));
        t.reshape(TensorShape(2*n, 2));
        return t;
    }

    // Generate an tensor that corresponds to a n-qubit Z-spider.
    static Tensor<T> zspider(const size_t& n, const Phase& phase) {
        Tensor<T> t = xt::zeros<DataType>(TensorShape(n, 2));
        if (n == 0) {
            t() = 1. + std::exp(1.0i * phase.toFloatType<T>());
        } else {
            t[TensorIndex(n, 0)] = 1.;
            t[TensorIndex(n, 1)] = std::exp(1.0i * phase.toFloatType<T>());
        }
        return t;
    }

    // Generate an tensor that corresponds to a n-qubit X-spider.
    static Tensor<T> xspider(const size_t& n, const Phase& phase) {
        Tensor<T> t0 = xt::ones<DataType>(TensorShape(n, 2));
        Tensor<T> ketMinus(TensorShape{2});
        ketMinus(0, 0) = 1.;
        ketMinus(0, 1) = -1.;
        Tensor<T> t1 = tensorPow(ketMinus, n);
        t0._tensor += t1._tensor * std::exp(1.0i * phase.toFloatType<T>());
        t0._tensor /= std::pow(std::sqrt(2), n);
        return t0;
    }

    // tensor-dot two tensors
    // dots the two tensors along the axes in ax1 and ax2
    static Tensor<T> tensordot(const Tensor<T>& t1, const Tensor<T>& t2,
                               const TensorAxisList& ax1 = {}, const TensorAxisList& ax2 = {}) {
        if (ax1.size() != ax2.size()) {
            throw std::invalid_argument("The two index orders should contain the same number of indices.");
        }
        Tensor<T> ret = xt::linalg::tensordot(t1._tensor, t2._tensor, ax1, ax2);
        return ret;
    }
    // tensor-dot a tensor between pairs of axes
    // dots the tensor  along the axes in ax1 and ax2
    static Tensor<T> selfTensordot(const Tensor<T>& t, const TensorAxisList& ax1 = {}, const TensorAxisList& ax2 = {});
    // Convert the tensor to a matrix, i.e., a 2D tensor according to the two axis lists.
    Tensor<T> toMatrix(const TensorAxisList& axin, const TensorAxisList& axout);
    // Rearrange the element of the tensor to a new shape
    void reshape(const TensorShape& shape) { _tensor = _tensor.reshape(shape); }

    // Rearrange the order of axes
    static Tensor<T> transpose(const Tensor& t, const TensorAxisList& perm) {
        return xt::transpose(t._tensor, perm);
    }

private:
    InternalType _tensor;
    // Returns true if two axis lists are disjoint
    static bool isDisjoint(const TensorAxisList& ax1, const TensorAxisList& ax2);
    // Returns true if two axis lists form a partition spanning axis 0 to n-1, where n is the dimension of the tensor.
    static bool isPartition(const Tensor<T>& t, const TensorAxisList& axin, const TensorAxisList& axout);
    // Concat Two Axis Orders
    static TensorAxisList concatAxisList(const TensorAxisList& ax1, const TensorAxisList& ax2);
    // Calculate tensor powers
    static Tensor<T> tensorPow(const Tensor<T>& t, size_t n);
};



//------------------------------
// Tensor manipulation functions
//------------------------------
template<typename T>
Tensor<T> Tensor<T>::selfTensordot(const Tensor<T>& t, const TensorAxisList& ax1, const TensorAxisList& ax2) {
    if (ax1.size() != ax2.size()) {
        throw std::invalid_argument("The two index orders should contain the same number of indices.");
    }
    if (!isDisjoint(ax1, ax2)) {
        throw std::invalid_argument("The two index orders should be disjoint.");
    }
    if (ax1.empty()) return t;
    auto tmp = Tensor<T>::identity(ax1.size());
    auto tmpOrder = TensorAxisList(ax1.size() + ax2.size(), 0);
    std::iota(tmpOrder.begin(), tmpOrder.end(), 0);
    return tensordot(t, tmp, concatAxisList(ax1, ax2), tmpOrder);
}


template<typename T>
Tensor<T> Tensor<T>::toMatrix(const TensorAxisList& axin, const TensorAxisList& axout) {
    if (!isPartition(*this, axin, axout)) {
        throw std::invalid_argument("The two axis lists should partition 0~(n-1).");
    }
    Tensor<T> ret = xt::transpose(_tensor, concatAxisList(axin, axout));
    ret._tensor.reshape({intPow(2, axin.size()), intPow(2, axout.size())});
    return ret;
}
//------------------------------
// Private member functions
//------------------------------

template<typename T>
bool Tensor<T>::isDisjoint(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    return std::find_first_of(ax1.begin(), ax1.end(), ax2.begin(), ax2.end()) == ax1.end();
}
template<typename T>
bool Tensor<T>::isPartition(const Tensor<T>& t, const TensorAxisList& axin, const TensorAxisList& axout) {
    if (!isDisjoint(axin, axout)) return false;
    if (axin.size() + axout.size() != t._tensor.dimension()) return false;
    return true;
}

template<typename T>
TensorAxisList Tensor<T>::concatAxisList(const TensorAxisList& ax1, const TensorAxisList& ax2) {
    TensorAxisList ret = ax1;
    ret.insert(ret.end(), ax2.begin(), ax2.end());
    return ret;
}

template<typename T>
Tensor<T> Tensor<T>::tensorPow(const Tensor<T>& t, size_t n) {
    if (n == 0) return xt::ones<DataType>(TensorShape(0, 2));
    if (n == 1) return t;
    auto tmp = tensorPow(t, n / 2);
    if (n % 2 == 0)
        return Tensor<T>::tensordot(tmp, tmp);
    else 
        return Tensor<T>::tensordot(t, Tensor<T>::tensordot(tmp, tmp));
}

#endif  // TENSOR_H