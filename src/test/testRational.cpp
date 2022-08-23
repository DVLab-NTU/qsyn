/****************************************************************************
  FileName     [ test.cpp ]
  PackageName  [ test ]
  Synopsis     [ Test program for rational number class ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2015-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "ratioNum.h"
#include <iomanip>

using namespace std;

int
main()
{
   RatioNum q1(6, 8), q2(2, 3), q3 = q1;
   cout << boolalpha;

   cout << q1 << ", " << q2 << endl;
   cout << q1 + q2 << endl;
   cout << q1 - q2 << endl;
   cout << q1 * q2 << endl;
   cout << q1 / q2 << endl;
   cout << (q1 <  q2) << endl;
   cout << (q1 == q2) << endl;
   cout << (q1 <  q3) << endl;
   cout << (q1 == q3) << endl;

   return 0;
}
