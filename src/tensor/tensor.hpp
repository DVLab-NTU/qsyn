/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define class Tensor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>
#include <fmt/ostream.h>

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

#include "./tensor_util.hpp"
#include "util/util.hpp"

namespace qsyn::tensor {

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
    friend bool is_partition(Tensor<U> const& t, TensorAxisList const& axin, TensorAxisList const& axout);

    Tensor<DT> to_matrix(TensorAxisList const& axin, TensorAxisList const& axout);

    template <typename U>
    friend Tensor<U> direct_sum(Tensor<U> const& t1, Tensor<U> const& t2);

    void reshape(TensorShape const& shape);
    Tensor<DT> transpose(TensorAxisList const& perm) const;
    void adjoint();

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
Tensor<DT> Tensor<DT>::to_matrix(TensorAxisList const& axin, TensorAxisList const& axout) {
    using dvlab::utils::int_pow;
    if (!is_partition(*this, axin, axout)) {
        throw std::invalid_argument("The two axis lists should partition 0~(n-1).");
    }
    Tensor<DT> t = xt::transpose(this->_tensor, concat_axis_list(axin, axout));
    t._tensor.reshape({int_pow(2, axin.size()), int_pow(2, axout.size())});
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
bool is_partition(Tensor<U> const& t, TensorAxisList const& axin, TensorAxisList const& axout) {
    if (!is_disjoint(axin, axout)) return false;
    if (axin.size() + axout.size() != t._tensor.dimension()) return false;
    return true;
}

}  // namespace qsyn::tensor

template <typename DT>
struct fmt::formatter<qsyn::tensor::Tensor<DT>> : fmt::ostream_formatter {};
