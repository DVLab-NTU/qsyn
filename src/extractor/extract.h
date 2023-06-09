/****************************************************************************
  FileName     [ extract.h ]
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef EXTRACT_H
#define EXTRACT_H

#include <cstddef>  // for size_t
#include <optional>
#include <set>

#include "device.h"
#include "duostra.h"
#include "m2.h"     // for M2
#include "qcir.h"   // for QCir
#include "zxDef.h"  // for EdgeType, EdgeType::HADAMARD

extern bool SORT_FRONTIER;
extern bool SORT_NEIGHBORS;
extern bool PERMUTE_QUBITS;
extern bool FILTER_DUPLICATED_CXS;
extern size_t BLOCK_SIZE;
extern size_t OPTIMIZE_LEVEL;
class ZXGraph;

class Extractor {
public:
    using Target = std::unordered_map<size_t, size_t>;
    using ConnectInfo = std::vector<std::set<size_t>>;
    Extractor(ZXGraph*, QCir* = nullptr, std::optional<Device> = std::nullopt);
    ~Extractor() {}

    bool toPhysical() { return _device.has_value(); }
    QCir* getLogical() { return _logicalCircuit; }

    void initialize(bool fromEmpty = true);
    QCir* extract();
    bool extractionLoop(size_t = size_t(-1));
    bool removeGadget(bool check = false);
    bool biadjacencyElimination(bool check = false);
    void columnOptimalSwap();
    void extractSingles();
    bool extractCZs(bool check = false);
    void extractCXs(size_t = 1);
    size_t extractHsFromM2(bool check = false);
    void cleanFrontier();
    void permuteQubit();

    void updateNeighbors();
    void updateGraphByMatrix(EdgeType = EdgeType::HADAMARD);
    void createMatrix();

    void prependSingleQubitGate(std::string, size_t, Phase);
    void prependDoubleQubitGate(std::string, const std::vector<size_t>&, Phase);
    void prependSeriesGates(const std::vector<Operation>&, const std::vector<Operation>& = {});
    void prependSwapGate(size_t, size_t, QCir*);
    bool frontierIsCleaned();
    bool axelInNeighbors();
    bool containSingleNeighbor();
    void printCXs();
    void printFrontier();
    void printNeighbors();
    void printAxels();
    void printMatrix() { _biAdjacency.printMatrix(); }

    std::vector<size_t> findMinimalSums(M2&, bool = false);
    std::vector<M2::Oper> greedyReduction(M2&);

private:
    size_t _cntCXIter;
    ZXGraph* _graph;
    QCir* _logicalCircuit;
    QCir* _physicalCircuit;
    std::optional<Device> _device;
    std::optional<Device> _deviceBackup;
    ZXVertexList _frontier;
    ZXVertexList _neighbors;
    ZXVertexList _axels;
    std::unordered_map<size_t, size_t> _qubitMap;  // zx to qc

    M2 _biAdjacency;
    std::vector<M2::Oper> _cnots;

    void blockElimination(M2&, size_t&, size_t);
    void blockElimination(size_t&, M2&, size_t&, size_t);
    std::vector<Operation> _DuostraAssigned;
    std::vector<Operation> _DuostraMapped;
    // NOTE - Use only in column optimal swap
    Target findColumnSwap(Target);
    ConnectInfo _rowInfo;
    ConnectInfo _colInfo;

    size_t _cntCXFiltered;
    size_t _cntSwap;

    std::vector<size_t> _initialPlacement;
};

#endif