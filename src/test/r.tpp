/****************************************************************************
  FileName     [ test.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for rational number class ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 8 ]
****************************************************************************/
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

#include "rationalNumber.h"
#include "phase.h"

using namespace std;

int main() {
    Rational q1(6, 8), q2(2, 3), q3 = q1, q4(1.25003), q5(1.25003f, 1e-2f);
    Phase p1(1.5), p2(1.57), p3(1.571), p4(1, 2), p5(7.854), p6 = 3 * p5;

    cout << Rational::toRational(0.51) << endl;
    cout << Rational::toRational(0.501) << endl;
    cout << Rational::toRational(0.5001) << endl;
    cout << Rational::toRational(0.50001) << endl;
    cout << p1 << endl;
    cout << p2 << endl;
    cout << p3 << endl;
    cout << p4 << endl;
    cout << p5 << endl;
    cout << setPhaseUnit(PhaseUnit::ONE);
    cout << p1 << endl;
    cout << p2 << endl;
    cout << p3 << endl;
    cout << p4 << endl;
    cout << p5 << endl;
    cout << setPhaseUnit(PhaseUnit::PI);
    cout << p1 << endl;
    cout << p2 << endl;
    cout << p3 << endl;
    cout << p4 << endl;
    cout << p5 << endl;
    cout << p6 << endl;
    cout << p6/p5 << endl;

    return 0;
}
