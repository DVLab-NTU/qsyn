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
    cout << boolalpha;

    cout << q1 << ", " << q2 << ", " << q4 << ", " << q5 << endl;
    cout << q1 + q2 << endl;
    cout << q1 - q2 << endl;
    cout << q1 * q2 << endl;
    cout << q1 / q2 << endl;
    cout << q1 / 2 << endl;
    cout << (q1 < q2) << endl;
    cout << (q1 == q2) << endl;
    cout << (q1 < q3) << endl;
    cout << (q1 == q3) << endl;
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

    try {
        cout << q3 / 0 << endl;
    } catch (std::overflow_error &e) {
        cerr << "[Error] " << e.what() << endl;
    }

    Phase p7;
    p7.fromString("1.57079632679");
    cout << p7 << endl;
    p7.fromString<float>("1.57079632679");
    cout << p7 << endl;
    p7.fromString<long double>("1.57079632679");
    cout << p7 << endl;


    return 0;
}
