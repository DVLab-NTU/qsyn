/****************************************************************************
  FileName     [ m2.h ]
  PackageName  [ m2 ]
  Synopsis     [ Define class m2 (Boolean Matrix) structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef M2_H
#define M2_H

#include <cstddef>  // for size_t
#include <utility>  // for pair
#include <vector>

#include "zxDef.h"

//------------------------------------------------------------------------
//   Define classes
//------------------------------------------------------------------------

// REVIEW - Change if bit > 64
class Row {
public:
    Row(size_t id, const std::vector<unsigned char>& r) : _row(r) {}
    ~Row() {}
    const std::vector<unsigned char>& getRow() const { return _row; }
    void setRow(std::vector<unsigned char> row) { _row = row; }
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
    std::vector<unsigned char> _row;
};

class M2 {
public:
    using Oper = std::pair<size_t, size_t>;
    M2() {}
    ~M2() {}

    void reset();
    void defaultInit();
    bool fromZXVertices(const ZXVertexList& frontier, const ZXVertexList& neighbors);
    const std::vector<Row>& getMatrix() { return _matrix; }
    const std::vector<Oper>& getOpers() { return _opStorage; }
    const Row& getRow(size_t r) { return _matrix[r]; }

    size_t numRows() const { return _matrix.size(); }
    size_t numCols() const { return _matrix[0].size(); }

    void clear() {
        _matrix.clear();
        _opStorage.clear();
    }

    bool xorOper(size_t ctrl, size_t targ, bool track = false);
    size_t gaussianElimSkip(size_t blockSize, bool fullReduced, bool track = true);
    bool gaussianElim(bool track = false, bool isAugmentedMatrix = false);
    bool gaussianElimAugmented(bool track = false);
    bool isSolvedForm() const;
    bool isAugmentedSolvedForm() const;
    void printMatrix() const;
    void printTrack() const;
    void appendOneHot(size_t idx);
    size_t filterDuplicatedOps();
    size_t opDepth();
    float denseRatio();
    void pushColumn();

    Row& operator[](const size_t& i) {
        return _matrix[i];
    }
    const Row& operator[](const size_t& i) const {
        return _matrix[i];
    }

private:
    std::vector<Row> _matrix;
    std::vector<Oper> _opStorage;
};

#endif  // M2_H