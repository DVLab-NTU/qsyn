/****************************************************************************
  FileName     [ extract.h ]
  PackageName  [ extractor ]
  Synopsis     [ graph extractor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef EXTRACT_H
#define EXTRACT_H

#include <iostream>
#include <set>
#include <unordered_map>
#include <vector>

#include "m2.h"
#include "ordered_hashset.h"
#include "qcir.h"
#include "simplify.h"
#include "zxDef.h"
#include "zxGraph.h"
#include "zxRules.h"

class Extractor;

class Extractor {
public:
    using Target = unordered_map<size_t, size_t>;
    using ConnectInfo = vector<set<size_t>>;
    Extractor(ZXGraph* g) {
        _graph = g;
        _circuit = new QCir(-1);
        initialize();
    }
    ~Extractor() {}

    void initialize();
    QCir* extract();

    bool removeGadget();
    void gaussianElimination();
    void columnOptimalSwap();
    void extractSingles();
    void extractCZs(size_t = 0);
    void extractCXs(size_t = 0);
    size_t extractHsFromM2();
    void cleanFrontier();
    void permuteQubit();

    void updateNeighbors();
    void updateGraphByMatrix(EdgeType = EdgeType::HADAMARD);

    bool containSingleNeighbor();
    void printFrontier();
    void printNeighbors();
    void printAxels();

private:
    ZXGraph* _graph;
    QCir* _circuit;
    ZXVertexList _frontier;
    ZXVertexList _neighbors;
    ZXVertexList _axels;
    unordered_map<size_t, size_t> _qubitMap;  // zx to qc

    M2 _biAdjacency;
    vector<Oper> _cnots;

    // NOTE - Use only in column optimal swap
    Target findColumnSwap(Target);
    ConnectInfo _rowInfo;
    ConnectInfo _colInfo;
};

#endif