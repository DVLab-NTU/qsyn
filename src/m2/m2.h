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
    unsigned char& back() { return _row.back(); }
    const unsigned char& back() const { return _row.back(); }

    bool isOneHot() const;
    bool isZeros() const;
    void printRow() const;

    void push_back(unsigned char i) { _row.push_back(i); }

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
    bool fromZXVertices(const ZXVertexList& frontier, const ZXVertexList& neighbors);
    const vector<Row>& getMatrix() { return _matrix; }
    const vector<Oper>& getOpers() { return _opStorage; }
    const Row& getRow(size_t r) { return _matrix[r]; }

    size_t numRows() const { return _matrix.size(); }
    size_t numCols() const { return _matrix.empty() ? 0 : _matrix[0].size(); }
    
    void clear() { _matrix.clear(); _opStorage.clear(); }

    bool xorOper(size_t ctrl, size_t targ, bool track = false);
    bool gaussianElimSkip(bool track = true);
    bool gaussianElim(bool track = false, bool isAugmentedMatrix = false);
    bool gaussianElimAugmented(bool track = false);
    bool isSolvedForm() const;
    bool isAugmentedSolvedForm() const;
    void printMatrix() const;
    void printTrack() const;
    void appendOneHot(size_t idx);

    Row& operator[](const size_t& i) {
        return _matrix[i];
    }
    const Row& operator[](const size_t& i) const {
        return _matrix[i];
    }


private:
    vector<Row> _matrix;
    vector<Oper> _opStorage;
};

#endif  // M2_H