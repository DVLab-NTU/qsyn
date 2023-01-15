/****************************************************************************
  FileName     [ testPhase.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for Phase class ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "phase.h"
// --- Always put catch2/catch.hpp after all other header files ---
#include "catch2/catch.hpp"
// ----------------------------------------------------------------
Phase p1, p2(9), p3(3, 2), p4(5, 2), p5(-2, 3);

TEST_CASE("Phases are initiated correctly", "[Phase]") {
    REQUIRE(p1.getRational() == Rational(0));
    REQUIRE(p2.getRational() == Rational(1));
    REQUIRE(p3.getRational() == Rational(3, 2));
    REQUIRE(p4.getRational() == Rational(1, 2));
    REQUIRE(p5.getRational() == Rational(4, 3));
}

TEST_CASE("Phases numbers are initiated from floating points correctly", "[Phase]") {
    // Give only number between [0, 2pi) for this test.
    double f1 = 0.500003, eps1 = 1e-6;
    float f2 = 2.7654321f, eps2 = 1e-2f;
    long double f3 = 1.234567890234567890L, eps3 = 1e-10L;

    Phase pD0(f1);
    REQUIRE(pD0.toDouble() == Approx(f1).margin(1e-4));
    Phase pD1(f1, eps1);
    REQUIRE(pD1.toDouble() == Approx(f1).margin(eps1));
    Phase pD2(f2, eps2);
    REQUIRE(pD2.toFloat() == Approx(f2).margin(eps2));
    Phase pD3(f3, eps3);
    REQUIRE(pD3.toLongDouble() == Approx(f3).margin(eps3));
}

TEST_CASE("Phase arithmetics works correctly", "[Phase]") {
    REQUIRE((p3 + p5) == Phase(5, 6));
    REQUIRE((p3 - p5) == Phase(1, 6));
    REQUIRE((p3 - p5) == Phase(1, 6));
    REQUIRE((p5 * 3) == Phase(0));
    REQUIRE((p5 * 7) == (7 * p5));
    REQUIRE((p3 / 2) == Phase(3, 4));
    REQUIRE((p3 / p5) == Rational(9, 8));
    REQUIRE_THROWS_AS((p2 / 0), std::overflow_error);
    REQUIRE_THROWS_AS((p4 / p1), std::overflow_error);
}

TEST_CASE("Phase comparisons work correctly", "[Phase]") {
    REQUIRE_FALSE((p3 + p5) == (p3 - p5));
    REQUIRE((p3 + p5) != (p3 - p5));
}

TEST_CASE("Phases are printed correctly", "[Phase]") {
    Catch::StringMaker<Phase> sm;
    std::cout << setPhaseUnit(PhaseUnit::PI);
    REQUIRE_THAT(sm.convert(p1), Catch::Matchers::Equals("0"));
    REQUIRE_THAT(sm.convert(p2), Catch::Matchers::Equals("\u03C0"));
    REQUIRE_THAT(sm.convert(p3), Catch::Matchers::Equals("3\u03C0/2"));
    REQUIRE_THAT(sm.convert(p4), Catch::Matchers::Equals("\u03C0/2"));
    REQUIRE_THAT(sm.convert(p5), Catch::Matchers::Equals("4\u03C0/3"));
}