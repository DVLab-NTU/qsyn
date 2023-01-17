/****************************************************************************
  FileName     [ lattice.h ]
  PackageName  [ lattice ]
  Synopsis     [ Define class Lattice and LTContainer structures ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef LATTICE_H
#define LATTICE_H

#include <cstddef>  // for size_t
#include <vector>

class ZXGraph;

class Lattice {
public:
    Lattice(unsigned r, unsigned c, int qs = -3, int qe = -3) : _row(r), _col(c) {
        _qStart = qs;
        _qEnd = qe;
    }
    ~Lattice() {}

    // Setter and Getter
    void setRow(unsigned r) { _row = r; }
    void setCol(unsigned c) { _col = c; }
    void setQStart(size_t qs) { _qStart = qs; }
    void setQEnd(size_t qe) { _qEnd = qe; }
    unsigned getRow() const { return _row; }
    unsigned getCol() const { return _col; }
    int getQStart() const { return _qStart; }
    int getQEnd() const { return _qEnd; }

    void printLT() const;

private:
    unsigned _row;
    unsigned _col;
    int _qStart;
    int _qEnd;
};

class LTContainer {
public:
    LTContainer(unsigned nr, unsigned nc) {
        for (size_t r = 0; r < nr; r++) {
            _container.push_back(std::vector<Lattice>());
            for (size_t c = 0; c < nc; c++) {
                _container.back().emplace_back(r, c);
            }
        }
    }
    ~LTContainer() {}

    void resize(unsigned r, unsigned c);
    void updateRC();

    void printLTC() const;
    void generateLTC(ZXGraph* g);
    void addCol2Right(int c);
    void addRow2Bottom(int r);

    size_t numRows() const { return _container.size(); }
    size_t numCols() const { return (_container.empty()) ? 0 : _container[0].size(); }

private:
    std::vector<std::vector<Lattice>> _container;
};

namespace std {
template <>
struct hash<pair<int, int>> {
    size_t operator()(const pair<int, int>& k) const {
        return (
            (hash<int>()(k.first) ^
             (hash<int>()(k.second) << 1)) >>
            1);
    }
};

}  // namespace std

#endif