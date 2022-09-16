/****************************************************************************
  FileName     [ tensor.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Tensor class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 9 ]
****************************************************************************/
#ifndef TENSOR_H
#define TENSOR_H

#include <concepts>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <complex>
#include <cassert>
#include <exception>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor-blas/xlinalg.hpp>
#include "phase.h"

template<typename T>
class Tensor {
    using DataType     = std::complex<T>;
    using InternalType = xt::xarray<DataType>;
    using ShapeType    = std::vector<size_t>;
    using IndexType    = std::vector<size_t>;
    using IndexOrderType    = std::vector<size_t>;
public:
    Tensor(const ShapeType& shape): _tensor(shape) {}
    Tensor(ShapeType&& shape): _tensor(shape) {}
    template<typename From> requires std::convertible_to<From, InternalType>
    Tensor(const From& internal): _tensor(internal) {}
    template<typename From> requires std::convertible_to<From, InternalType>
    Tensor(From&& internal): _tensor(internal) {}

    friend std::ostream& operator<< (std::ostream& os, const Tensor<T>& t) {
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

    DataType& operator[](const IndexType& i) {
        return _tensor[i];
    }
    const DataType& operator[](const IndexType& i) const {
        return _tensor[i];
    }

    static Tensor zspider(const size_t& n, const Phase& phase) {
        Tensor<T> t = xt::zeros<DataType>(ShapeType(n, 2));
        t[IndexType(n, 0)] = 1.;
        t[IndexType(n, 1)] = std::exp(1.0i * phase.toFloatType<T>());
        return t;
    }

    static Tensor xspider(const size_t& n, const Phase& phase) {
        Tensor<T> t0 = xt::ones<DataType>(ShapeType(n, 2));
        Tensor<T> ketMinus(ShapeType{2});
        ketMinus(0, 0) = 1.;
        ketMinus(0, 1) = -1.;
        Tensor<T> t1 = ketMinus;
        for (size_t i = 1; i < n; ++i) {
            t1 = Tensor::tensordot(t1, ketMinus);
        }
        t0._tensor += t1._tensor;
        t0._tensor /= std::pow(std::sqrt(2), n);
        return t0;
    }
    
    static Tensor<T> tensordot(const Tensor<T>& t1, const Tensor<T>& t2, 
                     const IndexOrderType& ax1 = {}, const IndexOrderType& ax2 = {}) {
        if (ax1.size() != ax2.size()) {
            throw std::invalid_argument("The two index orders should contain the same number of indices.");
        }
        Tensor<T> ret = xt::linalg::tensordot(t1._tensor, t2._tensor, ax1, ax2);
        return ret;
    }

    Tensor<T> toMatrix(const IndexOrderType& axin, const IndexOrderType& axout) {
        if (!isPartition(axin, axout)) {
            throw std::invalid_argument("The two axis lists should partition 0~(n-1).");
        }
        IndexOrderType perm = axin;
        perm.insert(perm.end(),axout.begin(), axout.end());
        Tensor<T> ret = xt::transpose(_tensor, perm);
        ret._tensor.reshape({subspaceVolume(axin), subspaceVolume(axout)});
        return ret;
    }

    void reshape(const ShapeType& shape) {
        _tensor = _tensor.reshape(shape);
    }

    static Tensor<T> transpose(const Tensor& t, const IndexOrderType& perm) {
        return xt::transpose(t._tensor, perm);
    }
private:
    InternalType _tensor;

    bool isPartition(const IndexOrderType& axin, const IndexOrderType& axout) {
        if (std::find_first_of(axin.begin(), axin.end(), axout.begin(), axout.end()) != axin.end()) return false;
        if (axin.size() + axout.size() != _tensor.dimension()) return false;
        return true;
    }

    size_t intPow(size_t base, size_t n) {
        if (n == 0) return 1;
        if (n == 1) return base;
        size_t tmp = intPow(base, n/2);
        if (n%2 == 0) return tmp * tmp;
        else          return base * tmp * tmp;
    }

    size_t subspaceVolume(const IndexOrderType& ax) {
        return intPow(2, ax.size());
    }

};



#endif // TENSOR_H