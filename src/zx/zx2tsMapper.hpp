/****************************************************************************
  FileName     [ zx2tsMapper.h ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZX-to-Tensor Mapper structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef ZX2TS_MAPPER_H
#define ZX2TS_MAPPER_H

#include <cstddef>
#include <vector>

#include "jthread/stop_token.hpp"
#include "tensor/qtensor.hpp"
#include "zx/zxDef.hpp"

class ZXGraph;
class ZXVertex;

class ZX2TSMapper {
public:
    using Frontiers = ordered_hashmap<EdgePair, size_t>;

    ZX2TSMapper() {}

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

    std::optional<QTensor<double>> map(ZXGraph const& zxgraph);

private:
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
    struct InOutAxisList {
        TensorAxisList inputs;
        TensorAxisList outputs;
    };
    InOutAxisList getAxisOrders(ZXGraph const& zxgraph);
};

QTensor<double> getTSform(ZXVertex* v);

#endif  // ZX2TS_MAPPER_H
