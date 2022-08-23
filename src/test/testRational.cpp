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

using namespace std;

int main() {
    Rational q1(6, 8), q2(2, 3), q3 = q1, q4(1.25003), q5(1.25003f, 1e-3f);
    cout << boolalpha;

    cout << q1 << ", " << q2 << ", " << q4 << ", " << q5 << endl;
    cout << q1 + q2 << endl;
    cout << q1 - q2 << endl;
    cout << q1 * q2 << endl;
    cout << q1 / q2 << endl;
    cout << (q1 < q2) << endl;
    cout << (q1 == q2) << endl;
    cout << (q1 < q3) << endl;
    cout << (q1 == q3) << endl;
    cout << Rational::approximate(0.51) << endl;
    cout << Rational::approximate(0.501) << endl;
    cout << Rational::approximate(0.5001) << endl;
    cout << Rational::approximate(0.50001) << endl;

    try {
        cout << q3 / 0 << endl;
    } catch (std::overflow_error &e) {
        cerr << "[Error] " << e.what() << endl;
    }

    return 0;
}
