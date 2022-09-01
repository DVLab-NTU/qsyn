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

Rational q1(6, 8), q2(2, 3), q3 = q1, q4, q5(9);

TEST_CASE("Rational Numbers are initiated correctly", "[Rational]") {
    REQUIRE(q1.numerator() == 3);
    REQUIRE(q1.denominator() == 4);
    REQUIRE(q2.numerator() == 2);
    REQUIRE(q2.denominator() == 3);
    REQUIRE(q3.numerator() == 3);
    REQUIRE(q3.denominator() == 4);
    REQUIRE(q4.numerator() == 0);
    REQUIRE(q4.denominator() == 1);
    REQUIRE(q5.numerator() == 9);
    REQUIRE(q5.denominator() == 1);
}

TEST_CASE("Rational Numbers are initiated from floating points correctly", "[Rational]") {
    Rational qD1(1.25003);
    REQUIRE(qD1.toDouble() == Approx(1.25003).epsilon(1e-4));
}

TEST_CASE("Rational Number arithmetics works correctly", "[Rational]") {
    REQUIRE((q1 + q2) == Rational(17, 12));
    REQUIRE((q1 - q2) == Rational(1, 12));
    REQUIRE((q1 * q2) == Rational(1, 2));
    REQUIRE((q1 / q2) == Rational(9, 8));
    REQUIRE((q1 / 2)  == Rational(3, 8));
    REQUIRE_THROWS_AS((q3 / 0), std::overflow_error);
}

TEST_CASE("Rational Number comparison works correctly", "[Rational]") {
    REQUIRE_FALSE(q1 < q2);
    REQUIRE_FALSE(q1 == q2);
    REQUIRE_FALSE(q1 < q3);
    REQUIRE(q1 == q3);
}