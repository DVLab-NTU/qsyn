#include <algorithm>
#include <cstddef>
#include <util/util.hpp>

#include "./rules/zx_rules_template.hpp"
#include "./simplify.hpp"
#include "zx/zx_partition.hpp"
#include "zx/zxgraph.hpp"

namespace qsyn::zx::simplify {

/**
 * @brief partition the graph into 2^numPartitions partitions and reduce each partition separately
 *        then merge the partitions together for n rounds (experimental)
 *
 * @param numPartitions number of partitions to create
 * @param iterations number of iterations
 */
void partition_reduce(ZXGraph& g, size_t n_partitions) {
    hadamard_rule_simp(g);
    auto const partitions        = kl_partition(g, n_partitions);
    auto const [subgraphs, cuts] = ZXGraph::create_subgraphs(std::move(g), partitions);

    for (auto& graph : subgraphs) {
        dynamic_reduce(*graph);
    }

    g = ZXGraph::from_subgraphs(subgraphs, cuts);

    spider_fusion_simp(g);
}

}  // namespace qsyn::zx::simplify
