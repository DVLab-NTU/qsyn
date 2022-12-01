/****************************************************************************
  FileName     [ m2.h ]
  PackageName  [ m2 ]
  Synopsis     [ Define m2 (Boolean Matrix) class ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef M2_H
#define M2_H

#include "m2Def.h"
#include <string>
#include <vector>
#include <iostream>

using namespace std;

class M2;
//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

class M2
{
public:
    M2()  {}
    ~M2() {}

    const vector<BitStr>& const getMatrix()     { return _matrix; }
    const vector<Oper>& const getOpers()        { return _opStorage; }
    const BitStr& const getRow(size_t r)        { return _matrix[r]; }
    void xorOper(size_t ctrl, size_t targ);
    void gaussianElim();
    bool isIdentity();

private:
    vector<BitStr>      _matrix; 
    vector<Oper>        _opStorage;

};

#endif // M2_H