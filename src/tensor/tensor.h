/****************************************************************************
  FileName     [ tensor.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of the Tensor package. ]
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

#include "nestedInitializerList.h"

class TensorShape;
class TensorStrides;

template <typename T>
class Tensor {
    using Shape   = std::vector<size_t>;
    using Strides = std::vector<size_t>;
public:
    Tensor(const Shape& shape, T value): _shape(shape) {
        size_t volume = std::reduce(_shape.begin(), _shape.end(), 1, std::multiplies<size_t>());
        _data.resize(volume);
        std::fill(_data.begin(), _data.end(), value);
        updateStrides();
    }
    //-------------------------
    // Get/Set
    //-------------------------
    
    // Printing the tensor
    friend std::ostream& operator<< (std::ostream& os, const Tensor& ts) {
        ts.printInternal(0, 0, os);
        return os;
    }

    // Element access
    T& operator() (const std::vector<size_t>& v) {
        if (v.size() != _shape.size()) throw std::invalid_argument("Wrong number of arguments");
        return _data[std::inner_product(v.begin(), v.end(), _strides.begin(), 0)];
    }

    // Get a string of the shape of the tensor
    std::string getShapeString() {
        std::string ret = "[" + std::to_string(_shape[0]);
        std::for_each(_shape.begin() + 1, _shape.end(), [&ret](const size_t& t) { ret += " " + std::to_string(t); });
        ret += "]";
        return ret;
    }

    // Get a string of the strides of the tensor
    std::string getStridesString() {
        std::string ret = "[" + std::to_string(_strides[0]);
        std::for_each(_strides.begin() + 1, _strides.end(), [&ret](const size_t& t) { ret += " " + std::to_string(t); });
        ret += "]";
        return ret;
    }
    
    // Return the the total number of elements of the tensor
    size_t size() {
        return _data.size();
    }
    // Get the shape of the tensor
    const std::vector<size_t>& getShape() const {
        return _shape;
    }
    // Get the strides of the tensor
    const std::vector<size_t>& getStrides() const {
        return _strides;
    }

    //-------------------------
    // Tensor manipulation
    //-------------------------

    void reshape(const std::vector<size_t>& newShape) {
        size_t newVolume = std::reduce(_shape.begin(), _shape.end(), 1, std::multiplies<size_t>());
        if (newVolume != this->size()) throw std::invalid_argument("Number of the elements in the tensor does not agree with the new shape");
        if (!strictlyDecreasingStrides()) {

        }
        _shape = newShape;
        updateStrides();
    }

    void transpose(const size_t& ax0, const size_t& ax1) {
        std::swap(_shape[ax0], _shape[ax1]);
        std::swap(_strides[ax0], _strides[ax1]);
    }

    //-------------------------
    // Element-wise arithmetics
    //-------------------------

    // In-place addition
    Tensor& operator+=(const Tensor& rhs) {
        if (_shape != rhs._shape) throw std::invalid_argument("Tensors of different shapes");
        for (int i = 0; i < this->size(); ++i) {
            _data[i] += rhs._data[i];
        }
        return *this;
    }

    // In-place subtraction
    Tensor& operator-=(const Tensor& rhs) {
        if (_shape != rhs._shape) throw std::invalid_argument("Tensors of different shapes");
        for (int i = 0; i < this->size(); ++i) {
            _data[i] -= rhs._data[i];
        }
        return *this;
    }

    // In-place multiplication
    Tensor& operator*=(const Tensor& rhs) {
        if (_shape != rhs._shape) throw std::invalid_argument("Tensors of different shapes");
        for (int i = 0; i < this->size(); ++i) {
            _data[i] *= rhs._data[i];
        }
        return *this;
    }

    // In-place division
    Tensor& operator/=(const Tensor& rhs) {
        if (_shape != rhs._shape) throw std::invalid_argument("Tensors of different shapes");
        for (int i = 0; i < this->size(); ++i) {
            if (rhs._data[i] == 0) throw std::overflow_error("Trying to divide by 0");
            _data[i] /= rhs._data[i];
        }
        return *this;
    }

    // In-place remainder
    Tensor& operator%=(const Tensor& rhs) {
        if (_shape != rhs._shape) throw std::invalid_argument("Tensors of different shapes");
        for (int i = 0; i < this->size(); ++i) {
            _data[i] %= rhs._data[i];
        }
        return *this;
    }

    // Addition
    friend Tensor operator+(Tensor lhs, const Tensor& rhs) {
        lhs += rhs;
        return lhs;
    }

    // Subtraction
    friend Tensor operator-(Tensor lhs, const Tensor& rhs) {
        lhs -= rhs;
        return lhs;
    }

    // Multiplication
    friend Tensor operator*(Tensor lhs, const Tensor& rhs) {
        lhs *= rhs;
        return lhs;
    }

    // Division
    friend Tensor operator/(Tensor lhs, const Tensor& rhs) {
        lhs /= rhs;
        return lhs;
    }

    // Remainder
    friend Tensor operator%(Tensor lhs, const Tensor& rhs) {
        lhs %= rhs;
        return lhs;
    }

    // FIXME: change to a 
    class Iterator: public std::iterator<std::bidirectional_iterator_tag, T> {
        friend class Tensor;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = value_type*;
        using reference         = value_type&;

    public:

        reference operator*() const { return *_ptr; }
        pointer   operator->() {return _ptr; }

        Iterator& operator++ () { _ptr++; return *this; } 
        Iterator  operator++ (int) { 
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        } 

        Iterator& operator-- () { _ptr--; return *this; } 
        Iterator  operator-- (int) { 
            Iterator tmp = *this;
            --(*this);
            return tmp;
        } 

        friend bool operator== (const Iterator& lhs, const Iterator& rhs) { return lhs._ptr == rhs._ptr; }
        friend bool operator!= (const Iterator& lhs, const Iterator& rhs) { return !(lhs == rhs); }


    private:
        Iterator(pointer ptr, Shape shape, Strides strides): _ptr(ptr), _shape(shape), _strides(strides) {
            
        }
        pointer _ptr;
        Shape   _shape;
        Strides _strides;
        std::vector<size_t> _pos;
    };
private:
    // FIXME: readability
    void printInternal(const size_t& depth, const size_t& start, std::ostream& os = std::cout) const {
        os << "[";
        if (depth == _shape.size() - 1) {
            os << _data[start];
            for (size_t i = 1; i < _shape[depth]; ++i) {
                os << " " << _data[start + i * _strides[depth]];
            }
        } else {
            printInternal(depth + 1, start, os);
            for (size_t i = 1; i < _shape[depth]; ++i) {
                os << "\n";
                for (size_t j = 0; j <= depth; ++j) {
                    os << " ";
                }
                printInternal(depth + 1, start + i * _strides[depth], os);
            }
        }
        os << "]";
    }

    void updateStrides() {
        _strides.resize(_shape.size());
        std::partial_sum(_shape.rbegin(), _shape.rend() - 1, _strides.rbegin() + 1, std::multiplies<size_t>());
        _strides.back() = 1;
    }

    bool strictlyDecreasingStrides() {
        for (int i = 1; i < _strides.size(); i++) {
            if (_strides[i - 1] < _strides[i]) return false;
        }
        return true;
    }
    

    std::vector<T> _data;
    Shape _shape;
    Strides _strides;
};

#endif // TENSOR_H