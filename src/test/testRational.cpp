/****************************************************************************
  FileName     [ testRational.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for rational number class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <sstream>
#include <string>

#include "rationalNumber.h"

// --- Always put catch2/catch.hpp after all other header files ---
#include "catch2/catch.hpp"
// ----------------------------------------------------------------

Rational q1(6, 8), q2(2, 3), q3 = q1, q4, q5(9), q6(1, 4);

TEST_CASE("Rational numbers are initiated correctly", "[Rational]") {
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

TEST_CASE("Rational numbers are initiated from floating points correctly", "[Rational]") {
    double f1 = 1.25003, eps1 = 1e-6;
    float f2 = 2.7654321f, eps2 = 1e-2f;
    long double f3 = 1.234567890234567890L, eps3 = 1e-10L;

    Rational qD0(f1);
    REQUIRE(qD0.toDouble() == Approx(f1).margin(1e-4));
    Rational qD1(f1, eps1);
    REQUIRE(qD1.toDouble() == Approx(f1).margin(eps1));
    Rational qD2(f2, eps2);
    REQUIRE(qD2.toFloat() == Approx(f2).margin(eps2));
    Rational qD3(f3, eps3);
    REQUIRE(qD3.toLongDouble() == Approx(f3).margin(eps3));
}

TEST_CASE("Rational number arithmetics works correctly", "[Rational]") {
    REQUIRE((q1 + q2) == Rational(17, 12));
    REQUIRE((q1 - q2) == Rational(1, 12));
    REQUIRE((q1 * q2) == Rational(1, 2));
    REQUIRE((q1 / q2) == Rational(9, 8));
    REQUIRE((q1 / 2) == Rational(3, 8));
    REQUIRE_THROWS_AS((q3 / 0), std::overflow_error);
    REQUIRE((q1 - q6) == Rational(1, 2));
}

TEST_CASE("Rational number comparisons work correctly", "[Rational]") {
    REQUIRE_FALSE(q1 < q2);
    REQUIRE_FALSE(q1 == q2);
    REQUIRE_FALSE(q1 < q3);
    REQUIRE(q1 == q3);
}

TEST_CASE("Rational number are printed correctly", "[Rational]") {
    Catch::StringMaker<Rational> sm;
    REQUIRE_THAT(sm.convert(q1), Catch::Matchers::Equals("3/4"));
    REQUIRE_THAT(sm.convert(q2), Catch::Matchers::Equals("2/3"));
    REQUIRE_THAT(sm.convert(q3), Catch::Matchers::Equals("3/4"));
    REQUIRE_THAT(sm.convert(q4), Catch::Matchers::Equals("0"));
    REQUIRE_THAT(sm.convert(q5), Catch::Matchers::Equals("9"));
}