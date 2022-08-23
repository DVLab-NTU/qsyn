/****************************************************************************
  FileName     [ myHashMap.h ]
  PackageName  [ util ]
  Synopsis     [ Define rational number and phase type]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#ifndef RATIONAL_NUM_H
#define RATIONAL_NUM_H

// TODO: define rational number class
#include <iostream>
#include <tuple>
class RatioNum {
public: 
    // Default constructor for two integral type
    RatioNum(int n, int d): _numer(n), _denom(d) { normalize(); }
    // Implicitly use 1 as denominator
    RatioNum(int n): _numer(n), _denom(1) {}
    RatioNum(const RatioNum&) = default;
    virtual ~RatioNum() = default;

    // Operator Overloading
    friend std::ostream& operator<< (std::ostream& os, const RatioNum& q);
    
    RatioNum& operator=(const RatioNum& rhs) = default;
    RatioNum& operator=(RatioNum&& rhs) = default;

    RatioNum operator+() const;
    RatioNum operator-() const;

    // Arithmetic operators always preserve the normalities of RatioNum
    RatioNum& operator+=(const RatioNum& rhs);
    friend RatioNum operator+(RatioNum lhs, const RatioNum& rhs); 
    RatioNum& operator-=(const RatioNum& rhs);
    friend RatioNum operator-(RatioNum lhs, const RatioNum& rhs); 
    RatioNum& operator*=(const RatioNum& rhs);
    friend RatioNum operator*(RatioNum lhs, const RatioNum& rhs); 
    RatioNum& operator/=(const RatioNum& rhs);
    friend RatioNum operator/(RatioNum lhs, const RatioNum& rhs); 

    bool operator== (const RatioNum& rhs) const;
    bool operator!= (const RatioNum& rhs) const;
    bool operator<  (const RatioNum& rhs) const;
    bool operator<= (const RatioNum& rhs) const;
    bool operator>  (const RatioNum& rhs) const;
    bool operator>= (const RatioNum& rhs) const;
  
    // Operations for Rational Numbers
    void normalize();
    void reciprocal(const RatioNum& q);

    
protected:
    int _numer, _denom;
};

#endif //RATIONAL_NUM_H