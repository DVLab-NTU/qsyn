/****************************************************************************
  FileName     [ lattice.cpp ]
  PackageName  [ lattice ]
  Synopsis     [ Define class Lattice / LTContainer member functions ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "lattice.h"

#include <iomanip>
#include <unordered_set>

#include "gFlow.h"
#include "textFormat.h"

namespace TF = TextFormat;

using namespace std;
extern size_t verbose;

void printMap(unordered_map<int, unordered_set<int>> map) {
    for (auto& s : map) {
        cout << s.first << ": ";
        for (auto& v : s.second) cout << v << ", ";
        cout << endl;
    }
}

void Lattice::printLT() const {
    cout << "( " << _row << ", " << _col << " ): " << _qStart << "/" << _qEnd << endl;
}

void LTContainer::resize(unsigned r, unsigned c) {
    _container.clear();
    for (size_t i = 0; i < r; i++) {
        _container.push_back(vector<Lattice>());
        for (size_t j = 0; j < c; j++) {
            _container.back().emplace_back(i, j);
        }
    }
}

void LTContainer::printLTC() const {
    for (size_t c = 0; c < numCols(); c++) {
        cout << setw(5) << right << c << setw(5) << right << "|";
    }
    cout << endl;
    for (auto& row : _container) {
        for (auto& lattice : row) {
            cout << setw(4) << right << ((lattice.getQStart() == -3) ? "-" : to_string(lattice.getQStart())) << "/"
                 << ((lattice.getQEnd() == -3) ? "-" : to_string(lattice.getQEnd())) << setw(4) << right << "|";
        }
        cout << endl;
    }
}

void LTContainer::updateRC() {
    for (size_t i = 0; i < numRows(); i++) {
        for (size_t j = 0; j < numCols(); j++) {
            _container[i][j].setRow(i);
            _container[i][j].setCol(j);
        }
    }
}

void LTContainer::addCol2Right(int c) {
    size_t iterOffset;
    if (c < 0) {
        iterOffset = 0;
    } else if (size_t(c) >= numCols()) {
        iterOffset = numCols();
    } else {
        iterOffset = c + 1;
    }

    for (size_t r = 0; r < numRows(); r++) {
        _container[r].emplace(_container[r].begin() + iterOffset, r, iterOffset);
    }

    if (iterOffset < numCols() - 1) {
        updateRC();
    }
}

void LTContainer::addRow2Bottom(int r) {
    size_t iterOffset = 0;
    if (r < 0) {
        iterOffset = 0;
    } else if (size_t(r) >= numRows()) {
        iterOffset = numRows();
    } else {
        iterOffset = r + 1;
    }

    auto nc = numCols(); 

    auto itr = _container.insert(_container.begin() + iterOffset, vector<Lattice>());
    for (size_t i = 0; i < nc; i++) {
        itr->emplace_back(iterOffset, i);
    }
    printLTC();
    
    if (iterOffset < numRows() - 1) {
        updateRC();
    }
}

void LTContainer::generateLTC(ZXGraph* g) {
    // Prerequisite:
    // Input col: 0
    // Output col: all equivalent and be the max col in `g`
    // Odd col: X , Even col: Z -> Col[1,2] as a unit
    // Not allow empty cols
    // `g` is consisted of several pairs of unit's composition

    using lsCoord = pair<int, int>;
    using qStartEnd = pair<int, int>;

    // ZXGraph* copyGraph = g->copy();

    // GFlow gflow(copyGraph);

    // gflow.calculate();

    // GFlow::Levels levels = gflow.getLevels();  // gflow levels are reversed to the measurement order
    // reverse(levels.begin(), levels.end());     // reverse to match measurement order
    // // set column
    // for (size_t i = 0; i < levels.size(); ++i) {
    //     for (auto& v : levels[i]) {
    //         v->setCol(i);
    //     }
    // }

    // for (size_t i = 0; i < levels.size() - 1; ++i) {
    //     for (auto& v : levels[i]) {
    //         for (auto [nb, etype] : v->getNeighbors()) {  // deliberately copy to avoid iterator invalidation
    //             if (nb->getCol() > i + 1) {
    //                 ZXVertex* buffer = g->addBuffer(nb, v, etype);
    //                 buffer->setCol(i + 1);
    //                 levels[i + 1].insert(buffer);
    //             }
    //         }
    //     }
    // }

    vector<vector<ZXVertex*>> colGroup;
    unsigned int maxCol = 0;
    for (auto& o : g->getOutputs()) {
        if (o->getCol() > maxCol) maxCol = o->getCol();
    }
    cout << "MaxCol: " << maxCol << endl;

    colGroup.resize(maxCol + 1);
    for (auto& v : g->getVertices()) {
        colGroup[v->getCol()].push_back(v);
    }
    // Print
    // for(size_t i = 0; i < colGroup.size(); i++){
    //     cout << "Col " << i << ": ";
    //     for(size_t j = 0; j < colGroup[i].size(); j++){
    //         cout << colGroup[i][j]->getId() << " ";
    //     }
    //     cout << endl;
    // }
    if (!(maxCol % 2)) return;
    for (unsigned int i = 1; i <= maxCol / 2; i++) {
        unordered_map<int, unordered_set<int>> start, end;

        // Generate start
        for (auto& v : colGroup[2 * i - 1]) {
            start[v->getQubit()] = unordered_set<int>();
            for (auto& n : v->getNeighbors()) {
                if (n.first->getCol() > v->getCol()) start[v->getQubit()].insert(n.first->getQubit());
            }
        }

        // Generate End
        for (auto& v : colGroup[2 * i]) {
            end[v->getQubit()] = unordered_set<int>();
            for (auto& n : v->getNeighbors()) {
                if (n.first->getCol() < v->getCol()) end[v->getQubit()].insert(n.first->getQubit());
            }
        }

        if (verbose > 3) {
            cout << "Start:" << endl;
            printMap(start);
            cout << "End:" << endl;
            printMap(end);
        }

        // Start mapping to LTC
        resize(end.size() + 1, start.size() + 1);
 
        unordered_map<qStartEnd, lsCoord> set;  // (qs, qe) -> (row, col)
        int count = 0;
        for (auto& [start_i, start_set] : start) {
            cout << "Start i: " << start_i << endl;
            for (auto& end_j : start_set) {
                set[make_pair(start_i, end_j)] = make_pair(count, -1);
            }
            count++;
        }
        count = 0;
        
        for (auto& [end_i, end_set] : end) {
            for (auto& start_j : end_set) {
                auto key = make_pair(start_j, end_i);
                if (set.contains(key))
                    set[key].second = count;
                else
                    set[key] = make_pair(-1, count);
            }
            count++;
        }
        
        vector<pair<qStartEnd, lsCoord>> sCand, eCand;
        for (auto& [key, value] : set) {
            auto& [qStart, qEnd] = key;
            auto& [lsCol, lsRow] = value;
            if (lsCol != -1 && lsRow != -1) {
                _container[lsRow][lsCol].setQStart(qStart);
                _container[lsRow][lsCol].setQEnd(qEnd);
            } else if (lsCol == -1)
                eCand.emplace_back(key, value);
            else if (lsRow == -1)
                sCand.emplace_back(key, value);
        }
        
        // Compensate vertically
        addRow2Bottom(-1);
        for (auto& [key, value] : sCand) {
            auto& [qStart, qEnd] = key;
            auto& [lsCol, lsRow] = value;
            if (verbose > 3) cout << qStart << "," << qEnd << ": (" << lsCol << "," << lsRow << ")\n";
            if (_container[0][lsCol].getQStart() == -3 && _container[0][lsCol].getQEnd() == -3) {
                // Find first not (-3, -3)
                for (size_t x = 1; x < numRows(); x++) {
                    if (_container[x][lsCol].getQStart() != -3 && _container[x][lsCol].getQEnd() != -3) {
                        _container[x - 1][lsCol].setQStart(qStart);
                        _container[x - 1][lsCol].setQEnd(qEnd);
                        break;
                    }
                }
            } else {
                // Find first (-3, -3)
                for (size_t x = 1; x < numRows(); x++) {
                    if (_container[x][lsCol].getQStart() == -3 && _container[x][lsCol].getQEnd() == -3) {
                        _container[x][lsCol].setQStart(qStart);
                        _container[x][lsCol].setQEnd(qEnd);
                        break;
                    }
                }
            }
        }

        

        // Compensate horizontally
        addCol2Right(-1);
        for (auto& [key, value] : eCand) {
            auto& [qStart, qEnd] = key;
            auto& [lsCol, lsRow] = value;
            lsRow++;
            cout << qStart << "," << qEnd << ": (" << lsCol << "," << lsRow << ")\n";
            for (size_t x = 1; x < numCols(); x++) {
                if (_container[lsRow][x].getQStart() != -3 && _container[lsRow][x].getQEnd() != -3) {
                    _container[lsRow][x - 1].setQStart(qStart);
                    _container[lsRow][x - 1].setQEnd(qEnd);
                    break;
                }
            }
        }
    }
}
