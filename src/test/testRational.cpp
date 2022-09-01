/****************************************************************************
  FileName     [ testRational.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for rational number class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/

#include "catch.hpp"

#include "rationalNumber.h"
#include "phase.h"

Rational q1(6, 8), q2(2, 3), q3 = q1;

TEST_CASE("Rational Numbers are initiated correctly", "[Rational]") {
    REQUIRE(q1.numerator() == 3);
    REQUIRE(q1.numerator() == 4);
}
