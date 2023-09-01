/****************************************************************************
  FileName     [ simpCmd.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define simplifier package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <string>

#include "simplifier/simplify.hpp"
#include "zx/zxCmd.hpp"
#include "zx/zxGraphMgr.hpp"

using namespace std;
using namespace ArgParse;
extern size_t verbose;

extern ZXGraphMgr zxGraphMgr;

Command ZXGSimpCmd();

bool initSimpCmd() {
    if (!cli.registerCommand("zxgsimp", 4, ZXGSimpCmd())) {
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validPreducePartitions = {
    [](size_t const &arg) {
        return (arg > 0);
    },
    [](size_t const &arg) {
        cerr << "The paritions parameter in partition reduce should be greater than 0" << endl;
    }};

ArgType<size_t>::ConstraintType validPreduceIteratoins = {
    [](size_t const &arg) {
        return (arg > 0);
    },
    [](size_t const &arg) {
        cerr << "The iterations parameter in partition reduce should be greater than 0" << endl;
    }};

//------------------------------------------------------------------------------------------------------------------
//    ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion |
//             -HOPF | -PIVOT | -LComp | -INTERClifford | -PIVOTGadget | -PIVOTBoundary | -CLIFford | -FReduce | -SReduce | -DReduce]
//------------------------------------------------------------------------------------------------------------------
Command ZXGSimpCmd() {
    return {"zxgsimp",
            zxGraphMgrNotEmpty,
            [](ArgumentParser &parser) {
                parser.description("perform simplification strategies for ZXGraph");

                auto mutex = parser.addMutuallyExclusiveGroup();
                mutex.addArgument<bool>("-dreduce", "--dynamic-reduce")
                    .action(storeTrue)
                    .help("perform dynamic full reduce");

                mutex.addArgument<bool>("-freduce", "--full-reduce")
                    .action(storeTrue)
                    .help("perform full reduce");

                mutex.addArgument<bool>("-sreduce", "--symbolic-reduce")
                    .action(storeTrue)
                    .help("perform symbolic reduce");

                mutex.addArgument<bool>("-preduce", "--partition-reduce")
                    .action(storeTrue)
                    .help("perform partition reduce");

                parser.addArgument<size_t>("p")
                    .nargs(NArgsOption::OPTIONAL)
                    .defaultValue(2)
                    .constraint(validPreducePartitions)
                    .help("the amount of partitions generated for preduce, defaults to 2");
                parser.addArgument<size_t>("n")
                    .nargs(NArgsOption::OPTIONAL)
                    .defaultValue(1)
                    .constraint(validPreduceIteratoins)
                    .help("the iterations parameter for preduce, defaults to 1");

                mutex.addArgument<bool>("-interclifford", "--interior-clifford")
                    .action(storeTrue)
                    .help("perform inter-clifford");

                mutex.addArgument<bool>("-clifford")
                    .action(storeTrue)
                    .help("perform clifford simplification");

                mutex.addArgument<bool>("-bialgebra")
                    .action(storeTrue)
                    .help("apply bialgebra rules");

                mutex.addArgument<bool>("-gadgetfusion")
                    .action(storeTrue)
                    .help("fuse phase gadgets connected to the same set of vertices");
                mutex.addArgument<bool>("-hfusion", "--hadamard-fusion")
                    .action(storeTrue)
                    .help("remove adjacent H-boxes or H-edges");
                mutex.addArgument<bool>("-hrule", "--hadamard-rule")
                    .action(storeTrue)
                    .help("convert H-boxes to H-edges");
                mutex.addArgument<bool>("-idremoval", "--identity-removal")
                    .action(storeTrue)
                    .help("remove Z/X-spiders with no phase and arity of 2");
                mutex.addArgument<bool>("-lcomp", "--local-complementation")
                    .action(storeTrue)
                    .help("apply local complementations to vertices with phase ±π/2");
                mutex.addArgument<bool>("-pivotrule")
                    .action(storeTrue)
                    .help("apply pivot rules to vertex pairs with phase 0 or π.");
                mutex.addArgument<bool>("-pivotboundary")
                    .action(storeTrue)
                    .help("apply pivot rules to vertex pairs connected to the boundary");
                mutex.addArgument<bool>("-pivotgadget")
                    .action(storeTrue)
                    .help("unfuse the phase and apply pivot rules to form gadgets");
                mutex.addArgument<bool>("-spiderfusion")
                    .action(storeTrue)
                    .help("fuse spiders of the same color");
                mutex.addArgument<bool>("-stcopy", "--state-copy")
                    .action(storeTrue)
                    .help("apply state copy rules");

                mutex.addArgument<bool>("-tograph")
                    .action(storeTrue)
                    .help("convert to green (Z) graph");

                mutex.addArgument<bool>("-torgraph")
                    .action(storeTrue)
                    .help("convert to red (X) graph");
            },
            [](ArgumentParser const &parser) {
                Simplifier s(zxGraphMgr.get());
                std::string procedure_str = "";
                if (parser.parsed("-sreduce")) {
                    s.symbolicReduce();
                    procedure_str = "SR";
                } else if (parser.parsed("-dreduce")) {
                    s.dynamicReduce();
                    procedure_str = "DR";
                } else if (parser.parsed("-preduce")) {
                    s.partitionReduce(parser.get<size_t>("p"), parser.get<size_t>("n"));
                    procedure_str = "PR";
                } else if (parser.parsed("-interclifford")) {
                    s.interiorCliffordSimp();
                    procedure_str = "INTERC";
                } else if (parser.parsed("-clifford")) {
                    s.cliffordSimp();
                    procedure_str = "CLIFF";
                } else if (parser.parsed("-bialgebra")) {
                    s.bialgSimp();
                    procedure_str = "BIALG";
                } else if (parser.parsed("-gadgetfusion")) {
                    s.gadgetSimp();
                    procedure_str = "GADFUS";
                } else if (parser.parsed("-hfusion")) {
                    s.hfusionSimp();
                    procedure_str = "HFUSE";
                } else if (parser.parsed("-hrule")) {
                    s.hruleSimp();
                    procedure_str = "HRULE";
                } else if (parser.parsed("-idremoval")) {
                    s.idSimp();
                    procedure_str = "IDRM";
                } else if (parser.parsed("-lcomp")) {
                    s.lcompSimp();
                    procedure_str = "LCOMP";
                } else if (parser.parsed("-pivotrule")) {
                    s.pivotSimp();
                    procedure_str = "PIVOT";
                } else if (parser.parsed("-pivotboundary")) {
                    s.pivotBoundarySimp();
                    procedure_str = "PVBND";
                } else if (parser.parsed("-pivotgadget")) {
                    s.pivotGadgetSimp();
                    procedure_str = "PVGAD";
                } else if (parser.parsed("-spiderfusion")) {
                    s.sfusionSimp();
                    procedure_str = "SPFUSE";
                } else if (parser.parsed("-stcopy")) {
                    s.copySimp();
                    procedure_str = "STCOPY";
                } else if (parser.parsed("-tograph")) {
                    s.toGraph();
                    procedure_str = "TOGRAPH";
                } else if (parser.parsed("-torgraph")) {
                    s.toRGraph();
                    procedure_str = "TORGRAPH";
                } else {
                    s.fullReduce();
                    procedure_str = "FR";
                }

                if (stop_requested()) {
                    procedure_str += "[INT]";
                }

                zxGraphMgr.get()->addProcedure(procedure_str);

                return CmdExecResult::DONE;
            }};
}
