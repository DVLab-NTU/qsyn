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
#include <iterator>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <tuple>
#include <complex>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor-blas/xlinalg.hpp>
#include <phase.h>

template<typename T>
class Tensor {
    using DataType     = std::complex<T>;
    using InternalType = xt::xarray<DataType>;
    using ShapeType    = std::vector<size_t>;
    using IndexType    = std::vector<size_t>;
public:
    Tensor(const ShapeType& shape): _tensor(shape) {}
    Tensor(ShapeType&& shape): _tensor(shape) {}
    Tensor(const InternalType& internal): _tensor(internal) {}
    Tensor(InternalType&& internal): _tensor(internal) {}

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
        Tensor<T> t(xt::zeros<DataType>(ShapeType(n, 2)));
        t[IndexType(n, 0)] = 1.;
        t[IndexType(n, 1)] = std::exp(1.0i * phase.toFloatType<T>());
        return t;
    }

    static Tensor xspider(const size_t& n, const Phase& phase) {
        
    }
private:
    InternalType _tensor;

};



#endif // TENSOR_H