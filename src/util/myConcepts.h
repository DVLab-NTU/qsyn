/****************************************************************************
  FileName     [ phase.cpp ]
  PackageName  [ util ]
  Synopsis     [ User-defined concepts ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/

#ifndef MY_CONCEPTS_H
#define MY_CONCEPTS_H

#include <concepts>

#include "rationalNumber.h"

template <class T>
concept Unitless = requires(T t) {
                       std::integral<T> == true ||
                           std::floating_point<T> == true ||
                           std::same_as<T, Rational> == true;
                   };
#endif  // MY_CONCEPTS_H