/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Define simplifier package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./simp_cmd.hpp"

#include <cstddef>
#include <string>

#include "./simplify.hpp"
#include "cli/cli.hpp"
#include "zx/zx_cmd.hpp"
#include "zx/zxgraph.hpp"
#include "zx/zxgraph_mgr.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::zx {

extern size_t VERBOSE;

bool valid_partition_reduce_partitions(size_t const &n_parts) {
    if (n_parts > 0) return true;
    std::cerr << "The paritions parameter in partition reduce should be greater than 0" << std::endl;
    return false;
};

//------------------------------------------------------------------------------------------------------------------
//    ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion |
//             -HOPF | -PIVOT | -LComp | -INTERClifford | -PIVOTGadget | -PIVOTBoundary | -CLIFford | -FReduce | -SReduce | -DReduce]
//------------------------------------------------------------------------------------------------------------------
Command zxgraph_simplify_cmd(zx::ZXGraphMgr &zxgraph_mgr) {
    return {"zxgsimp",
            [](ArgumentParser &parser) {
                parser.description("perform simplification strategies for ZXGraph");

                auto mutex = parser.add_mutually_exclusive_group();
                mutex.add_argument<bool>("-dreduce", "--dynamic-reduce")
                    .action(store_true)
                    .help("perform dynamic full reduce");

                mutex.add_argument<bool>("-freduce", "--full-reduce")
                    .action(store_true)
                    .help("perform full reduce");

                mutex.add_argument<bool>("-sreduce", "--symbolic-reduce")
                    .action(store_true)
                    .help("perform symbolic reduce");

                mutex.add_argument<bool>("-preduce", "--partition-reduce")
                    .action(store_true)
                    .help("perform partition reduce");

                parser.add_argument<size_t>("p")
                    .nargs(NArgsOption::optional)
                    .default_value(2)
                    .constraint(valid_partition_reduce_partitions)
                    .help("the amount of partitions generated for preduce, defaults to 2");

                mutex.add_argument<bool>("-interclifford", "--interior-clifford")
                    .action(store_true)
                    .help("perform inter-clifford");

                mutex.add_argument<bool>("-clifford")
                    .action(store_true)
                    .help("perform clifford simplification");

                mutex.add_argument<bool>("-bialgebra")
                    .action(store_true)
                    .help("apply bialgebra rules");

                mutex.add_argument<bool>("-gadgetfusion")
                    .action(store_true)
                    .help("fuse phase gadgets connected to the same set of vertices");
                mutex.add_argument<bool>("-hfusion", "--hadamard-fusion")
                    .action(store_true)
                    .help("remove adjacent H-boxes or H-edges");
                mutex.add_argument<bool>("-hrule", "--hadamard-rule")
                    .action(store_true)
                    .help("convert H-boxes to H-edges");
                mutex.add_argument<bool>("-idremoval", "--identity-removal")
                    .action(store_true)
                    .help("remove Z/X-spiders with no phase and arity of 2");
                mutex.add_argument<bool>("-lcomp", "--local-complementation")
                    .action(store_true)
                    .help("apply local complementations to vertices with phase ±π/2");
                mutex.add_argument<bool>("-pivotrule")
                    .action(store_true)
                    .help("apply pivot rules to vertex pairs with phase 0 or π.");
                mutex.add_argument<bool>("-pivotboundary")
                    .action(store_true)
                    .help("apply pivot rules to vertex pairs connected to the boundary");
                mutex.add_argument<bool>("-pivotgadget")
                    .action(store_true)
                    .help("unfuse the phase and apply pivot rules to form gadgets");
                mutex.add_argument<bool>("-spiderfusion")
                    .action(store_true)
                    .help("fuse spiders of the same color");
                mutex.add_argument<bool>("-stcopy", "--state-copy")
                    .action(store_true)
                    .help("apply state copy rules");

                mutex.add_argument<bool>("-tograph")
                    .action(store_true)
                    .help("convert to green (Z) graph");

                mutex.add_argument<bool>("-torgraph")
                    .action(store_true)
                    .help("convert to red (X) graph");
            },
            [&](ArgumentParser const &parser) {
                if (!zx::zxgraph_mgr_not_empty(zxgraph_mgr)) return dvlab::CmdExecResult::error;
                zx::Simplifier s(zxgraph_mgr.get());
                std::string procedure_str = "";
                if (parser.parsed("-sreduce")) {
                    s.symbolic_reduce();
                    procedure_str = "SR";
                } else if (parser.parsed("-dreduce")) {
                    s.dynamic_reduce();
                    procedure_str = "DR";
                } else if (parser.parsed("-preduce")) {
                    s.partition_reduce(parser.get<size_t>("p"));
                    procedure_str = "PR";
                } else if (parser.parsed("-interclifford")) {
                    s.interior_clifford_simp();
                    procedure_str = "INTERC";
                } else if (parser.parsed("-clifford")) {
                    s.clifford_simp();
                    procedure_str = "CLIFF";
                } else if (parser.parsed("-bialgebra")) {
                    s.bialgebra_simp();
                    procedure_str = "BIALG";
                } else if (parser.parsed("-gadgetfusion")) {
                    s.phase_gadget_simp();
                    procedure_str = "GADFUS";
                } else if (parser.parsed("-hfusion")) {
                    s.hadamard_fusion_simp();
                    procedure_str = "HFUSE";
                } else if (parser.parsed("-hrule")) {
                    s.hadamard_rule_simp();
                    procedure_str = "HRULE";
                } else if (parser.parsed("-idremoval")) {
                    s.identity_removal_simp();
                    procedure_str = "IDRM";
                } else if (parser.parsed("-lcomp")) {
                    s.local_complement_simp();
                    procedure_str = "LCOMP";
                } else if (parser.parsed("-pivotrule")) {
                    s.pivot_simp();
                    procedure_str = "PIVOT";
                } else if (parser.parsed("-pivotboundary")) {
                    s.pivot_boundary_simp();
                    procedure_str = "PVBND";
                } else if (parser.parsed("-pivotgadget")) {
                    s.pivot_gadget_simp();
                    procedure_str = "PVGAD";
                } else if (parser.parsed("-spiderfusion")) {
                    s.spider_fusion_simp();
                    procedure_str = "SPFUSE";
                } else if (parser.parsed("-stcopy")) {
                    s.state_copy_simp();
                    procedure_str = "STCOPY";
                } else if (parser.parsed("-tograph")) {
                    s.to_z_graph();
                    procedure_str = "TOGRAPH";
                } else if (parser.parsed("-torgraph")) {
                    s.to_x_graph();
                    procedure_str = "TORGRAPH";
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

bool add_zx_simplifier_cmds(dvlab::CommandLineInterface &cli, zx::ZXGraphMgr &zxgraph_mgr) {
    if (!cli.add_command(zxgraph_simplify_cmd(zxgraph_mgr))) {
        std::cerr << "Registering \"zx\" commands fails... exiting" << std::endl;
        return false;
    }
    return true;
}

}  // namespace qsyn::zx
