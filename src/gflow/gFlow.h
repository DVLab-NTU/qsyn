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
    using CorrectionSetMap = std::unordered_map<ZXVertex*, ZXVertexList>;
    enum class MeasurementPlane {
        XY,
        YZ,
        XZ,
        NOT_A_QUBIT,
        ERROR
    };
    using MeasurementPlaneMap = std::unordered_map<ZXVertex*, MeasurementPlane>;

    GFlow(ZXGraph* g) : _zxgraph{g}, _valid{false}, _doIndependentLayers{false}, _doExtended{true} {}

    bool calculate();

    Levels const& getLevels() const { return _levels; }
    CorrectionSetMap const& getCorrectionSets() const { return _correctionSets; }
    MeasurementPlaneMap const& getMeasurementPlanes() const { return _measurementPlanes; }

    bool isValid() const { return _valid; }

    void doIndependentLayers(bool flag) { _doIndependentLayers = flag; }
    void doExtendedGFlow(bool flag) { _doIndependentLayers = flag; }

    void print() const;
    void printLevels() const;
    void printCorrectionSets() const;
    void printCorrectionSet(ZXVertex* v) const;
    void printSummary() const;
    void printFailedVertices() const;

private:
    ZXGraph* _zxgraph;
    Levels _levels;
    CorrectionSetMap _correctionSets;
    std::unordered_map<ZXVertex*, MeasurementPlane> _measurementPlanes;

    bool _valid;
    bool _doIndependentLayers;
    bool _doExtended;

    // helper members
    ZXVertexList _frontier;
    ZXVertexList _neighbors;
    std::unordered_set<ZXVertex*> _taken;
    M2 _coefficientMatrix;

    // gflow calculation subroutines
    void initialize();
    void calculateZerothLayer();
    void updateNeighborsByFrontier();
    M2 prepareMatrix(ZXVertex* v, size_t i);
    void setCorrectionSetFromMatrix(ZXVertex* v, const M2& matrix);
    void updateFrontier();

    void printFrontier() const;
    void printNeighbors() const;
};

std::ostream& operator<<(std::ostream& os, GFlow::MeasurementPlane const& plane);

#endif  // GFLOW_H