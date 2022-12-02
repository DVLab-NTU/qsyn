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
#include <bitset>
#include <assert.h>

using namespace std;

class M2;
class Row;
//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

//REVIEW - Change if bit > 64
class Row
{
public:
    Row(size_t id): _id(id) {}
    ~Row() {}
    const bitset<64>& getRow()            { return _row; }
    void setRow(bitset<64> row)           { _row = row; }

    size_t size()                         { return _row.size(); }
    bool isSingular()                     { return _row.count() == 1; }
    
    Row &operator+=(const Row& rhs);
private:
    size_t _id;
    bitset<64> _row;
};

class M2
{
public:
    M2()  {}
    ~M2() {}

    //NOTE - Initializer
    bool fromZXVertices();

    const vector<Row>& getMatrix()          { return _matrix; }
    const vector<Oper>& getOpers()          { return _opStorage; }
    const Row getRow(size_t r)              { return _matrix[r]; }
    void xorOper(size_t ctrl, size_t targ)  { _matrix[targ] += _matrix[ctrl]; }
    void gaussianElim();
    bool isIdentity();

private:
    vector<Row>         _matrix; 
    vector<Oper>        _opStorage;

};

Row operator+(Row& lhs, const Row& rhs){
  lhs += rhs;
  return lhs;
}

Row &Row::operator+=(const Row& rhs) {
    assert( _row.size() == rhs._row.size());
    _row ^= rhs._row;
    return *this;
}
#endif // M2_H