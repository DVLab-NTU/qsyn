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
#include <set>

#include "m2.h"     // for M2
#include "qcir.h"   // for QCir
#include "topology.h"
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
    Extractor(ZXGraph* g, QCir* c = nullptr, DeviceTopo* dt = nullptr) {
        _graph = g;
        _device = dt;
        if (_device != nullptr) {
            std::cout << std::endl;
            std::cout << "Extracting to device" << std::endl;
            std::cout << std::endl;
            _device->printTopo();
        }
        if (c == nullptr)
            _circuit = new QCir(-1);
        else
            _circuit = c;
        initialize(c == nullptr);
        _cntCXFiltered = 0;

        _cntCXIter = 0;
    }
    ~Extractor() {}

    void initialize(bool fromEmpty = true);
    QCir* extract();
    bool extractionLoop(size_t = size_t(-1));
    bool removeGadget(bool check = false);
    bool gaussianElimination(bool check = false);
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

    bool frontierIsCleaned();
    bool axelInNeighbors();
    bool containSingleNeighbor();
    void printCXs();
    void printFrontier();
    void printNeighbors();
    void printAxels();
    void printMatrix() { _biAdjacency.printMatrix(); }

private:
    size_t _cntCXIter;
    ZXGraph* _graph;
    QCir* _circuit;
    DeviceTopo* _device;
    ZXVertexList _frontier;
    ZXVertexList _neighbors;
    ZXVertexList _axels;
    std::unordered_map<size_t, size_t> _qubitMap;  // zx to qc

    M2 _biAdjacency;
    std::vector<M2::Oper> _cnots;

    void blockElimination(M2&, size_t&, size_t);
    // NOTE - Use only in column optimal swap
    Target findColumnSwap(Target);
    ConnectInfo _rowInfo;
    ConnectInfo _colInfo;

    size_t _cntCXFiltered;
};

#endif