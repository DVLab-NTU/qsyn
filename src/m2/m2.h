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
#include <algorithm>
#include <bitset>
#include <assert.h>
#include "zxGraph.h"

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
    Row(size_t id, size_t l, const vector<unsigned char>& r): _id(id), _size(l), _row(r) { reverse(_row.begin(), _row.end()); }
    ~Row() {}
    const vector<unsigned char>& getRow()               { return _row; }
    void setRow(vector<unsigned char> row)              { _row = row; }
    size_t size()                                       { return _size; }
    bool isOneHot();

    void printRow() const;

    Row &operator+=(const Row& rhs);
private:
    size_t _id;
    size_t _size;
    vector<unsigned char> _row;
};

class M2
{
public:
    M2(size_t n): _size(n) {}
    ~M2() {}

    //NOTE - Initializer
    void defaultInit();
    bool fromZXVertices(const vector<ZXVertex*>&, const vector<ZXVertex*>&);
    const vector<Row>& getMatrix()          { return _matrix; }
    const vector<Oper>& getOpers()          { return _opStorage; }
    const Row getRow(size_t r)              { return _matrix[r]; }
    bool xorOper(size_t ctrl, size_t targ, bool track=false);
    void gaussianElim(bool track=false);
    bool isIdentity();
    void printMatrix() const;
    void printTrack() const;

private:
    size_t              _size;
    vector<Row>         _matrix; 
    vector<Oper>        _opStorage;
    bool inFrontier(ZXVertex*, const vector<ZXVertex*>&);
};

#endif // M2_H