/****************************************************************************
  FileName     [ zx2tsMapper.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZX-to-Tensor Mapper structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef ZX2TS_MAPPER_H
#define ZX2TS_MAPPER_H

#include <cstddef>  // for size_t

#include "qtensor.h"  // for QTensor
#include "zxDef.h"    // for NeighborPair

class ZXGraph;
class ZXVertex;

class ZX2TSMapper {
public:
    using Frontiers = ordered_hashmap<EdgePair, size_t>;

    ZX2TSMapper(ZXGraph* zxg) : _zxgraph(zxg) {}
    class ZX2TSList {
    public:
        const Frontiers& frontiers(const size_t& id) const {
            return _zx2tsList[id].first;
        }
        const QTensor<double>& tensor(const size_t& id) const {
            return _zx2tsList[id].second;
        }
        Frontiers& frontiers(const size_t& id) {
            return _zx2tsList[id].first;
        }
        QTensor<double>& tensor(const size_t& id) {
            return _zx2tsList[id].second;
        }
        void append(const Frontiers& f, const QTensor<double>& q) {
            _zx2tsList.emplace_back(f, q);
        }
        size_t size() {
            return _zx2tsList.size();
        }

    private:
        std::vector<std::pair<Frontiers, QTensor<double>>> _zx2tsList;
    };

    bool map();

private:
    ZXGraph* _zxgraph;                     // The ZX Graph to be mapped
    std::vector<EdgePair> _boundaryEdges;  // EdgePairs of the boundaries
    ZX2TSList _zx2tsList;                  // The tensor list for each set of frontiers
    size_t _tensorId;                      // Current tensor id for the _tensorId

    TensorAxisList _simplePins;          // Axes that can be tensordotted directly
    TensorAxisList _hadamardPins;        // Axes that should be applied hadamards first
    std::vector<EdgePair> _removeEdges;  // Old frontiers to be removed
    std::vector<EdgePair> _addEdges;     // New frontiers to be added

    Frontiers& currFrontiers() { return _zx2tsList.frontiers(_tensorId); }
    QTensor<double>& currTensor() { return _zx2tsList.tensor(_tensorId); }
    const Frontiers& currFrontiers() const { return _zx2tsList.frontiers(_tensorId); }
    const QTensor<double>& currTensor() const { return _zx2tsList.tensor(_tensorId); }

    void mapOneVertex(ZXVertex* v);

    // mapOneVertex Subroutines
    void initSubgraph(ZXVertex* v);
    void tensorDotVertex(ZXVertex* v);
    void updatePinsAndFrontiers(ZXVertex* v);
    QTensor<double> dehadamardize(const QTensor<double>& ts);

    bool isOfNewGraph(const ZXVertex* v);
    bool isFrontier(const NeighborPair& nbr) const;

    void printFrontiers(size_t id) const;
    // EdgePair makeEdgeKey(ZXVertex* v1, ZXVertex* v2, EdgeType* et);
    void getAxisOrders(TensorAxisList& inputAxisList, TensorAxisList& outputAxisList);
};
#endif  // ZX2TS_MAPPER_H