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

#include "./simplify.hpp"
#include "argparse/arg_parser.hpp"
#include "cli/cli.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "zx/zx_cmd.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_mgr.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::zx {

bool valid_partition_reduce_partitions(size_t const &n_parts) {
    if (n_parts > 0) return true;
    spdlog::error("The paritions parameter in partition reduce should be greater than 0");
    return false;
};

Command zxgraph_optimize_cmd(zx::ZXGraphMgr &zxgraph_mgr) {
    return {"optimize",
            [](ArgumentParser &parser) {
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
            },
            [&](ArgumentParser const &parser) {
                if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return dvlab::CmdExecResult::error;
                zx::Simplifier s(zxgraph_mgr.get());
                std::string procedure_str = "";

                if (parser.parsed("--symbolic")) {
                    s.symbolic_reduce();
                    procedure_str = "SR";
                } else if (parser.parsed("--dynamic")) {
                    s.dynamic_reduce();
                    procedure_str = "DR";
                } else if (parser.parsed("--partition")) {
                    s.partition_reduce(parser.get<size_t>("--partition"));
                    procedure_str = "PR";
                } else if (parser.parsed("--interior-clifford")) {
                    s.interior_clifford_simp();
                    procedure_str = "ICR";
                } else if (parser.parsed("--clifford")) {
                    s.clifford_simp();
                    procedure_str = "CR";
                } else {
                    s.full_reduce();
                    procedure_str = "FR";
                }

                if (stop_requested()) {
                    procedure_str += "[INT]";
                }

                zxgraph_mgr.get()->add_procedure(procedure_str);
                return CmdExecResult::done;
            }};
}

Command zxgraph_rule_cmd(zx::ZXGraphMgr &zxgraph_mgr) {
    return Command{
        "rule",
        [](ArgumentParser &parser) {
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
        [&](ArgumentParser const &parser) {
            if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return dvlab::CmdExecResult::error;
            zx::Simplifier s(zxgraph_mgr.get());

            if (parser.parsed("--bialgebra")) {
                s.bialgebra_simp();
            } else if (parser.parsed("--gadget-fusion")) {
                s.phase_gadget_simp();
            } else if (parser.parsed("--hadamard-fusion")) {
                s.hadamard_fusion_simp();
            } else if (parser.parsed("--hadamard-rule")) {
                s.hadamard_rule_simp();
            } else if (parser.parsed("--identity-removal")) {
                s.identity_removal_simp();
            } else if (parser.parsed("--local-complementation")) {
                s.local_complement_simp();
            } else if (parser.parsed("--pivot")) {
                s.pivot_simp();
            } else if (parser.parsed("--pivot-boundary")) {
                s.pivot_boundary_simp();
            } else if (parser.parsed("--pivot-gadget")) {
                s.pivot_gadget_simp();
            } else if (parser.parsed("--spider-fusion")) {
                s.spider_fusion_simp();
            } else if (parser.parsed("--state-copy")) {
                s.state_copy_simp();
            } else if (parser.parsed("--to-z-graph")) {
                s.to_z_graph();
            } else if (parser.parsed("--to-x-graph")) {
                s.to_x_graph();
            } else {
                spdlog::error("No rule specified");
                return CmdExecResult::error;
            }
            return CmdExecResult::done;
        }};
}

Command zxgraph_manual_apply_cmd(zx::ZXGraphMgr &zxgraph_mgr) {
    return Command{
        "manual",
        [&](ArgumentParser &parser) {
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
        [&](ArgumentParser const &parser) {
            if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return dvlab::CmdExecResult::error;
            zx::Simplifier s(zxgraph_mgr.get());

            // if (parser.parsed("--pivot")) {
            //     s.pivot_simp();
            // } else if (parser.parsed("--pivot-boundary")) {
            //     s.pivot_boundary_simp();
            // } else if (parser.parsed("--pivot-gadget")) {
            //     s.pivot_gadget_simp();
            // } else if (parser.parsed("--spider-fusion")) {
            //     s.spider_fusion_simp();
            // } else {
            //     spdlog::error("No rule specified");
            //     return CmdExecResult::error;
            // }
            auto vertices = parser.get<std::vector<size_t>>("vertices");
            // fmt::println("{} {}", vertices[0], vertices[1]);
            return CmdExecResult::done;
        }};
}
}  // namespace qsyn::zx
