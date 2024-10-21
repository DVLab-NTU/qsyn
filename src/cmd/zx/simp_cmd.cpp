/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define simplifier package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./simp_cmd.hpp"

#include <fmt/core.h>

#include <cstddef>
#include <string>
#include <vector>

#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "cmd/zx_cmd.hpp"
#include "cmd/zxgraph_mgr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "zx/simplifier/simplify.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_action.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::zx {

bool valid_partition_reduce_partitions(size_t const& n_parts) {
    if (n_parts > 0) return true;
    spdlog::error("The partitions parameter in partition reduce should be greater than 0");
    return false;
};

Command zxgraph_optimize_cmd(zx::ZXGraphMgr& zxgraph_mgr) {
    return {"optimize",
            [](ArgumentParser& parser) {
                parser.description("perform optimization routines for ZXGraph");

                auto mutex = parser.add_mutually_exclusive_group();
                mutex.add_argument<bool>("-f", "--full")
                    .action(store_true)
                    .help("Runs full reduction routine. This is the default routine.");
                mutex.add_argument<bool>("-d", "--dynamic")
                    .action(store_true)
                    .help("Runs full reduction routine, but stops early when T-count stops decreasing and the graph density starts increasing.");
                mutex.add_argument<bool>("-s", "--symbolic")
                    .action(store_true)
                    .help("Runs an optimization that is suitable for symbolically calculating output states given input states.");
                mutex.add_argument<size_t>("-p", "--partition")
                    .metavar("#partitions")
                    .default_value(2)
                    .nargs(NArgsOption::optional)
                    .constraint(valid_partition_reduce_partitions)
                    .help("Partitions the graph into `#partitions` subgraphs and runs full reduction on each of them.");
                mutex.add_argument<bool>("-i", "--interior-clifford")
                    .action(store_true)
                    .help("Runs reduction to the interior of the ZXGraph without producing phase gadgets");
                mutex.add_argument<bool>("-c", "--clifford")
                    .action(store_true)
                    .help("Runs reduction without producing phase gadgets");
                mutex.add_argument<size_t>("-C", "--causal")
                    .default_value(2)
                    .help(
                        "Runs a causal flow-preserving routine that reduces "
                        "2Q-counts. The parameter is the maximum number of "
                        "LCompUnfusion and PivotUnfusion to apply.");
            },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return dvlab::CmdExecResult::error;
                std::string procedure_str = "";

                if (parser.parsed("--symbolic")) {
                    simplify::symbolic_reduce(*zxgraph_mgr.get());
                    procedure_str = "SR";
                } else if (parser.parsed("--dynamic")) {
                    simplify::dynamic_reduce(*zxgraph_mgr.get());
                    procedure_str = "DR";
                } else if (parser.parsed("--partition")) {
                    simplify::partition_reduce(*zxgraph_mgr.get(), parser.get<size_t>("--partition"));
                    procedure_str = "PR";
                } else if (parser.parsed("--interior-clifford")) {
                    simplify::interior_clifford_simp(*zxgraph_mgr.get());
                    procedure_str = "ICR";
                } else if (parser.parsed("--clifford")) {
                    simplify::clifford_simp(*zxgraph_mgr.get());
                    procedure_str = "CR";
                } else if (parser.parsed("--causal")) {
                    auto const max_unfusions = parser.get<size_t>("--causal");
                    simplify::causal_flow_opt(*zxgraph_mgr.get(),
                                              max_unfusions, max_unfusions);
                    procedure_str = fmt::format("Causal-{}", max_unfusions);
                } else {
                    simplify::full_reduce(*zxgraph_mgr.get());
                    procedure_str = "FR";
                }

                if (stop_requested()) {
                    procedure_str += "[INT]";
                }

                zxgraph_mgr.get()->add_procedure(procedure_str);
                return CmdExecResult::done;
            }};
}

Command zxgraph_rule_cmd(zx::ZXGraphMgr& zxgraph_mgr) {
    return Command{
        "rule",
        [](ArgumentParser& parser) {
            parser.description("apply simplification rules to ZXGraph");

            auto mutex = parser.add_mutually_exclusive_group().required(true);
            mutex.add_argument<bool>("--bialgebra")
                .action(store_true)
                .help("applies bialgebra rules");
            mutex.add_argument<bool>("--gadget-fusion")
                .action(store_true)
                .help("fuses phase gadgets connected to the same set of vertices");
            mutex.add_argument<bool>("--hadamard-fusion")
                .action(store_true)
                .help("removes adjacent H-boxes or H-edges");
            mutex.add_argument<bool>("--hadamard-rule")
                .action(store_true)
                .help("converts H-boxes to H-edges");
            mutex.add_argument<bool>("--identity-removal")
                .action(store_true)
                .help("removes Z/X-spiders with no phase and arity of 2");
            mutex.add_argument<bool>("--local-complementation")
                .action(store_true)
                .help("applies local complementations to vertices with phase ±π/2");
            mutex.add_argument<bool>("--pivot")
                .action(store_true)
                .help("applies pivot rules to vertex pairs with phase 0 or π");
            mutex.add_argument<bool>("--pivot-boundary")
                .action(store_true)
                .help("applies pivot rules to vertex pairs connected to the boundary");
            mutex.add_argument<bool>("--pivot-gadget")
                .action(store_true)
                .help("unfuses the phase and applies pivot rules to form gadgets");
            mutex.add_argument<bool>("--spider-fusion")
                .action(store_true)
                .help("fuses spiders of the same color");
            mutex.add_argument<bool>("--state-copy")
                .action(store_true)
                .help("applies state copy rules to eliminate gadgets with phase 0 or π");
            mutex.add_argument<bool>("--to-z-graph")
                .action(store_true)
                .help("convert all X-spiders to Z-spiders");
            mutex.add_argument<bool>("--to-x-graph")
                .action(store_true)
                .help("convert all Z-spiders to X-spiders");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return dvlab::CmdExecResult::error;

            if (parser.parsed("--bialgebra")) {
                simplify::bialgebra_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--gadget-fusion")) {
                simplify::phase_gadget_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--hadamard-fusion")) {
                simplify::hadamard_fusion_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--hadamard-rule")) {
                simplify::hadamard_rule_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--identity-removal")) {
                simplify::identity_removal_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--local-complementation")) {
                simplify::local_complement_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--pivot")) {
                simplify::pivot_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--pivot-boundary")) {
                simplify::pivot_boundary_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--pivot-gadget")) {
                simplify::pivot_gadget_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--spider-fusion")) {
                simplify::spider_fusion_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--state-copy")) {
                simplify::state_copy_simp(*zxgraph_mgr.get());
            } else if (parser.parsed("--to-z-graph")) {
                simplify::to_z_graph(*zxgraph_mgr.get());
            } else if (parser.parsed("--to-x-graph")) {
                simplify::to_x_graph(*zxgraph_mgr.get());
            } else {
                spdlog::error("No rule specified");
                return CmdExecResult::error;
            }
            return CmdExecResult::done;
        }};
}

// REVIEW - Logic of check function is not completed
Command zxgraph_manual_apply_cmd(zx::ZXGraphMgr& zxgraph_mgr) {
    return Command{
        "manual",
        [&](ArgumentParser& parser) {
            parser.description("apply simplification rules on specific candidates");

            auto mutex = parser.add_mutually_exclusive_group().required(true);
            mutex.add_argument<bool>("--pivot")
                .action(store_true)
                .help("applies pivot rules to vertex pairs with phase 0 or π");
            mutex.add_argument<bool>("--pivot-boundary")
                .action(store_true)
                .help("applies pivot rules to vertex pairs connected to the boundary");
            mutex.add_argument<bool>("--pivot-gadget")
                .action(store_true)
                .help("unfuses the phase and applies pivot rules to form gadgets");

            parser.add_argument<size_t>("vertices")
                .nargs(2)
                .constraint(valid_zxvertex_id(zxgraph_mgr))
                .help("the vertices on which the rule applies");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return dvlab::CmdExecResult::error;
            auto vertices   = parser.get<std::vector<size_t>>("vertices");
            ZXVertex* bound = (*zxgraph_mgr.get())[vertices[0]];
            ZXVertex* vert  = (*zxgraph_mgr.get())[vertices[1]];

            const bool is_cand = PivotBoundaryRule().is_candidate(*zxgraph_mgr.get(), bound, vert);
            if (!is_cand) return CmdExecResult::error;

            PivotUnfusion const pvu{bound->get_id(), vert->get_id(), {}, {}};
            pvu.apply(*zxgraph_mgr.get());
            return CmdExecResult::done;
        }};
}
}  // namespace qsyn::zx
