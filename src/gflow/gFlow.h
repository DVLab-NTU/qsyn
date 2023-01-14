/****************************************************************************
  FileName     [ gFlow.h ]
  PackageName  [ gflow ]
  Synopsis     [ Define class GFlow structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef GFLOW_H
#define GFLOW_H

#include <unordered_map>
#include <vector>

#include "m2.h"
#include "zxGraph.h"

class ZXGraph;
class ZXVertex;

class GFlow {
public:
    using Levels = std::vector<ZXVertexList>;
    using CorrectionSets = std::unordered_map<ZXVertex*, ZXVertexList>;

    GFlow(ZXGraph* g) : _zxgraph(g), _valid(false) {}

    void reset();
    bool calculate(bool disjointNeighbors = false);

    const Levels& getLevels() const { return _levels; }
    bool isValid() const { return _valid; }

    void print() const;
    void printLevels() const;
    void printCorrectionSets() const;
    void printCorrectionSet(ZXVertex* v) const;
    void printSummary() const;
    void printFailedVertices() const;

private:
    ZXGraph* _zxgraph;
    Levels _levels;
    CorrectionSets _correctionSets;

    bool _valid;

    // helper members
    ZXVertexList _frontier;
    ZXVertexList _neighbors;
    std::unordered_set<ZXVertex*> _taken;
    M2 _coefficientMatrix;

    // gflow calculation subroutines
    void clearTemporaryStorage();
    void calculateZerothLayer();
    void updateNeighborsByFrontier();
    void setCorrectionSetFromMatrix(ZXVertex* v, const M2& matrix);
    void updateFrontier();

    void printFrontier() const;
    void printNeighbors() const;
};

#endif  // GFLOW_H