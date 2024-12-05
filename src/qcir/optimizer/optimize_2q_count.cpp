#include "extractor/extract.hpp"
#include "qcir/optimizer/optimizer.hpp"
#include "zx/simplifier/simplify.hpp"

namespace qsyn::qcir {

void optimize_2q_count(QCir& qcir,
                       double hadamard_insertion_ratio,
                       size_t max_lc_unfusions,
                       size_t max_pv_unfusions) {
    using namespace zx::simplify;
    using namespace std::chrono;
    // print the graph density and max degree
    auto const get_max_degree_vertex =
        [](zx::ZXGraph const& g) -> zx::ZXVertex* {
        if (g.is_empty()) return nullptr;
        return *std::ranges::max_element(
            g.get_vertices(), std::ranges::less{},
            [&](auto const& v) { return g.num_neighbors(v); });
    };

    auto const to_ms = [](auto const& d) {
        return duration_cast<milliseconds>(d).count();
    };

    auto twoq_count      = get_gate_statistics(qcir).at("2-qubit");
    auto round           = 0;
    auto acc_round_stuck = 0l;
    auto acc_to_zxgraph  = 0l;
    auto acc_causal      = 0l;
    auto acc_extract     = 0l;
    auto acc_opt         = 0l;
    auto start_time      = steady_clock::now();
    while (true) {
        auto const loop_start_time = high_resolution_clock::now();
        auto zx                    = to_zxgraph(qcir);
        if (!zx) {
            spdlog::error("Fail to convert QCir to ZXGraph.");
            return;
        }
        auto const causal_start_time = high_resolution_clock::now();
        redundant_hadamard_insertion(*zx, hadamard_insertion_ratio);
        to_graph_like(*zx);
        auto const max_degree = zx->num_neighbors(get_max_degree_vertex(*zx));
        causal_flow_opt(*zx, max_lc_unfusions, max_pv_unfusions);

        auto const extract_start_time = high_resolution_clock::now();
        extractor::Extractor ext{&(*zx), extractor::ExtractorConfig{}};
        ext.extract();
        qcir = std::move(*ext.get_logical());

        auto const opt_start_time = high_resolution_clock::now();
        Optimizer opt;
        opt.basic_optimization(qcir, Optimizer::BasicOptimizationConfig{});

        auto new_twoq_count = get_gate_statistics(qcir).at("2-qubit");
        if (new_twoq_count >= twoq_count) {
            acc_round_stuck++;
            hadamard_insertion_ratio *= 0.95;
            if (acc_round_stuck >= 10) {
                break;
            }
        } else {
            acc_round_stuck = 0;
        }

        if (max_degree >= 40) {
            hadamard_insertion_ratio *= 1.05;
        }

        twoq_count = new_twoq_count;
        round++;
        acc_to_zxgraph += to_ms(causal_start_time - loop_start_time);
        acc_causal += to_ms(extract_start_time - causal_start_time);
        acc_extract += to_ms(opt_start_time - extract_start_time);
        acc_opt += to_ms(high_resolution_clock::now() - opt_start_time);
    }

    fmt::println("{:>4}: 2Q-count = {:>6}, Temp = {:>.6}",
                 round, twoq_count, hadamard_insertion_ratio);
    auto const total_time = acc_to_zxgraph + acc_causal + acc_extract + acc_opt;
    fmt::println(
        "Time: {:>6}ms (total), {:>6}ms (to zxgraph), {:>6}ms (causal), "
        "{:>6}ms (extract), {:>6}ms (opt)",
        total_time, acc_to_zxgraph, acc_causal,
        acc_extract, acc_opt);
    // also print percentage
}

}  // namespace qsyn::qcir
