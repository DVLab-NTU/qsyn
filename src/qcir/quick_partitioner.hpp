/****************************************************************************
  PackageName  [ qcir / quick_partitioner ]
  Synopsis     [ BQSKit QuickPartitioner-style unitary-region partitioning. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <set>
#include <vector>

#include "qcir/qcir.hpp"

namespace qsyn::qcir {

enum class PartitionStrategy {
    Quick,       // BQSKit QuickPartitioner only
    Scan,        // ScanPartitioner-lite (maximize MQ gates per region)
    QuickScan,   // Quick + merge + scan refinement (default)
};

// One contiguous unitary region (topological gate order preserved).
struct UnitaryRegion {
    std::vector<QCirGate const*> gates;
    std::set<QubitIdType>        qubits;
};

// BQSKit QuickPartitioner: single forward scan, bin gates while
// |union(qubits)| <= block_size (default 3).
[[nodiscard]] std::vector<UnitaryRegion>
quick_partition(QCir const& circuit, size_t block_size);

// Graph refinement: build gate adjacency (consecutive gates sharing a qubit),
// then merge adjacent regions when the merged qubit set still fits in block_size.
[[nodiscard]] std::vector<UnitaryRegion>
quick_partition_with_merge(QCir const& circuit, size_t block_size);

// Split a region at 2-qubit gates (and trailing 1q after entangling) so native
// KAK/QSD does not see arbitrary RZ–CX compositions as one SU(4) blob.
[[nodiscard]] std::vector<UnitaryRegion>
split_entangling_boundaries(UnitaryRegion const& region);

// Gate interaction graph: consecutive gates sharing a qubit are adjacent.
// Greedily pick highest-scoring contiguous window with <= block_size qubits.
[[nodiscard]] std::vector<UnitaryRegion>
scan_partition(QCir const& circuit, size_t block_size);

[[nodiscard]] std::vector<UnitaryRegion>
partition_circuit(QCir const& circuit, size_t block_size, PartitionStrategy strategy);

}  // namespace qsyn::qcir
