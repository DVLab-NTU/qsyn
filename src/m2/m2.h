/****************************************************************************
  FileName     [ m2.h ]
  PackageName  [ m2 ]
  Synopsis     [ Define m2 (Boolean Matrix) class ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef M2_H
#define M2_H

#include <assert.h>

#include <algorithm>
#include <bitset>
#include <iostream>
#include <string>
#include <vector>

#include "m2Def.h"
#include "zxGraph.h"

using namespace std;

class M2;
class Row;
//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

// REVIEW - Change if bit > 64
class Row {
public:
    Row(size_t id, const vector<unsigned char>& r) : _id(id), _row(r) {}
    ~Row() {}
    const vector<unsigned char>& getRow() const { return _row; }
    void setRow(vector<unsigned char> row) { _row = row; }
    size_t size() const { return _row.size(); }
    bool isOneHot() const;

    void printRow() const;

    Row& operator+=(const Row& rhs);

    unsigned char& operator[](const size_t& i) {
        return _row[i];
    }
    const unsigned char& operator[](const size_t& i) const {
        return _row[i];
    }

private:
    size_t _id;
    vector<unsigned char> _row;
};

class M2 {
public:
    M2() {}
    ~M2() {}

    // NOTE - Initializer
    void reset();
    void defaultInit();
    bool fromZXVertices(const ZXVertexList&, const ZXVertexList&);
    const vector<Row>& getMatrix() { return _matrix; }
    const vector<Oper>& getOpers() { return _opStorage; }
    const Row& getRow(size_t r) { return _matrix[r]; }
    bool xorOper(size_t ctrl, size_t targ, bool track = false);
    void gaussianElim(bool track = false);
    bool isIdentity() const;
    void printMatrix() const;
    void printTrack() const;

private:
    vector<Row> _matrix;
    vector<Oper> _opStorage;
};

#endif  // M2_H