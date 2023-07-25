/****************************************************************************
  FileName     [ simpCmd.cpp ]
  PackageName  [ simplifier ]
  Synopsis     [ Define simplifier package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "qcirCmd.h"
#include "qcirMgr.h"
#include "simplify.h"
#include "zxCmd.h"
#include "zxGraphMgr.h"
#include "zxoptimizer.h"

using namespace std;
using namespace ArgParse;
extern size_t verbose;

extern ZXGraphMgr zxGraphMgr;
ZXOPTimizer opt;

unique_ptr<ArgParseCmdType> ZXGSimpCmd();
unique_ptr<ArgParseCmdType> ZXOPTCmd();
unique_ptr<ArgParseCmdType> ZXOPTPrintCmd();
unique_ptr<ArgParseCmdType> ZXOPTR2rCmd();
unique_ptr<ArgParseCmdType> ZXOPTS2sCmd();

bool initSimpCmd() {
    // OPTimizer opt;
    if (!(
            cmdMgr->regCmd("ZXGSimp", 4, ZXGSimpCmd()) &&
            cmdMgr->regCmd("ZXOPT", 5, ZXOPTCmd()) &&
            cmdMgr->regCmd("ZXOPTPrint", 6, ZXOPTPrintCmd()) &&
            cmdMgr->regCmd("ZXOPTR2r", 6, ZXOPTR2rCmd()) &&
            cmdMgr->regCmd("ZXOPTS2s", 6, ZXOPTS2sCmd()))) {
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validPreduceSliceRounds = {
    [](ArgType<size_t> &arg) {
        return [&arg]() {
            return (arg.getValue() <= 10 && arg.getValue() >= 1);
        };
    },
    [](ArgType<size_t> const &arg) {
        return [&arg]() {
            cerr << "The sliceTime parameter in partition reduce should be in the range of [1, 10]" << endl;
        };
    }};

ArgType<size_t>::ConstraintType validPreduceIteratoins = {
    [](ArgType<size_t> &arg) {
        return [&arg]() {
            return (arg.getValue() <= 10 && arg.getValue() >= 1);
        };
    },
    [](ArgType<size_t> const &arg) {
        return [&arg]() {
            cerr << "The rounds parameter in partition reduce should be in the range of [1, 10]" << endl;
        };
    }};

//------------------------------------------------------------------------------------------------------------------
//    ZXGSimp [-TOGraph | -TORGraph | -HRule | -SPIderfusion | -BIAlgebra | -IDRemoval | -STCOpy | -HFusion |
//             -HOPF | -PIVOT | -LComp | -INTERClifford | -PIVOTGadget | -PIVOTBoundary | -CLIFford | -FReduce | -SReduce | -DReduce]
//------------------------------------------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXGSimpCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXGSimp");

    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("perform simplification strategies for ZXGraph");

        auto mutex = parser.addMutuallyExclusiveGroup();
        mutex.addArgument<bool>("-dreduce")
            .action(storeTrue)
            .help("perform dynamic full reduce");

        mutex.addArgument<bool>("-freduce")
            .action(storeTrue)
            .help("perform full reduce");

        mutex.addArgument<bool>("-sreduce")
            .action(storeTrue)
            .help("perform symbolic reduce");
        mutex.addArgument<bool>("-preduce")
            .action(storeTrue)
            .help("perform partition reduce");

        parser.addArgument<size_t>("p")
            .required(false)
            .defaultValue(1)
            .constraint(validPreduceSliceRounds)
            .help("the amount of partitions generated for preduce");
        parser.addArgument<size_t>("n")
            .required(false)
            .defaultValue(1)
            .constraint(validPreduceIteratoins)
            .help("the iterations parameter for preduce, if not specified, it will be set to 1");

        mutex.addArgument<bool>("-interclifford")
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
        mutex.addArgument<bool>("-hfusion")
            .action(storeTrue)
            .help("remove adjacent H-boxes or H-edges");
        mutex.addArgument<bool>("-hrule")
            .action(storeTrue)
            .help("convert H-boxes to H-edges");
        mutex.addArgument<bool>("-idremoval")
            .action(storeTrue)
            .help("remove Z/X-spiders with no phase and arity of 2");
        mutex.addArgument<bool>("-lcomp")
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
        // mutex.addArgument<bool>("-degadgetize")
        //     .action(storeTrue)
        //     .help("[UNSTABLE!] apply unfusions and pivot rules so that the resulting graph has no gadgets");
        mutex.addArgument<bool>("-spiderfusion")
            .action(storeTrue)
            .help("fuse spiders of the same color");
        mutex.addArgument<bool>("-stcopy")
            .action(storeTrue)
            .help("apply state copy rules");

        mutex.addArgument<bool>("-tograph")
            .action(storeTrue)
            .help("convert to green (Z) graph");

        mutex.addArgument<bool>("-torgraph")
            .action(storeTrue)
            .help("convert to red (X) graph");
    };

    cmd->onParseSuccess = [](std::stop_token st, ArgumentParser const &parser) {
        ZX_CMD_GRAPHMGR_NOT_EMPTY_OR_RETURN("ZXGSimp");
        Simplifier s(zxGraphMgr.get(), st);
        if (parser["-sreduce"].isParsed())
            s.symbolicReduce();
        else if (parser["-dreduce"].isParsed())
            s.hybridReduce();
        else if (parser["-preduce"].isParsed()) {
            s.partitionReduce(parser["p"], parser["n"]);
        } else if (parser["-interclifford"].isParsed())
            s.interiorCliffordSimp();
        else if (parser["-clifford"].isParsed())
            s.cliffordSimp();
        else if (parser["-bialgebra"].isParsed())
            s.bialgSimp();
        else if (parser["-gadgetfusion"].isParsed())
            s.gadgetSimp();
        else if (parser["-hfusion"].isParsed())
            s.hfusionSimp();
        else if (parser["-hrule"].isParsed())
            s.hruleSimp();
        else if (parser["-idremoval"].isParsed())
            s.idSimp();
        else if (parser["-lcomp"].isParsed())
            s.lcompSimp();
        else if (parser["-pivotrule"].isParsed())
            s.pivotSimp();
        else if (parser["-pivotboundary"].isParsed())
            s.pivotBoundarySimp();
        else if (parser["-pivotgadget"].isParsed())
            s.pivotGadgetSimp();
        // else if (parser["-degadgetize"].isParsed())
        //     s.degadgetizeSimp(st);
        else if (parser["-spiderfusion"].isParsed())
            s.sfusionSimp();
        else if (parser["-stcopy"].isParsed())
            s.copySimp();
        else if (parser["-tograph"].isParsed())
            s.toGraph();
        else if (parser["-torgraph"].isParsed())
            s.toRGraph();
        else
            s.fullReduce();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

//------------------------------------------------------------------------------------------------------------------
//    ZXOPT
//------------------------------------------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXOPTCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXOPT");
    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("Dynamic Optimization for a ZXGraph");
    };
    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        opt.myOptimize();
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//------------------------------------------------------------------------------------------------------------------
//    ZXOPTPrint [-Gadgetfusion | -Spiderfusion | -Idremoval | -PIVOTRule | -Lcomp | -PIVOTGadget | -PIVOTBoundary ]
//------------------------------------------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXOPTPrintCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXOPTPrint");
    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("print parameter of optimizer for ZXGraph");

        auto mutex = parser.addMutuallyExclusiveGroup();

        mutex.addArgument<bool>("-idremoval")
            .action(storeTrue)
            .help("perform identity removal");

        mutex.addArgument<bool>("-lcomp")
            .action(storeTrue)
            .help("perform local complementation");

        mutex.addArgument<bool>("-gadgetfusion")
            .action(storeTrue)
            .help("perform gadget fusion");

        mutex.addArgument<bool>("-pivotrule")
            .action(storeTrue)
            .help("perform pivot");

        mutex.addArgument<bool>("-pivotboundary")
            .action(storeTrue)
            .help("perform pivot boundary");

        mutex.addArgument<bool>("-pivotgadget")
            .action(storeTrue)
            .help("perform pivot gadget");

        mutex.addArgument<bool>("-spiderfusion")
            .action(storeTrue)
            .help("perform spider fusion");

        mutex.addArgument<bool>("-interclifford")
            .action(storeTrue)
            .help("perform inter-clifford");

        mutex.addArgument<bool>("-clifford")
            .action(storeTrue)
            .help("perform clifford");
    };
    cmd->onParseSuccess = [](std::stop_token st, ArgumentParser const &parser) {
        if (parser["-idremoval"].isParsed())
            opt.printSingle("Identity Removal Rule");
        else if (parser["-lcomp"].isParsed())
            opt.printSingle("Local Complementation Rule");
        else if (parser["-gadgetfusion"].isParsed())
            opt.printSingle("Phase Gadget Rule");
        else if (parser["-pivotrule"].isParsed())
            opt.printSingle("Pivot Rule");
        else if (parser["-pivotboundary"].isParsed())
            opt.printSingle("Pivot Boundary Rule");
        else if (parser["-pivotgadget"].isParsed())
            opt.printSingle("Pivot Gadget Rule");
        else if (parser["-spiderfusion"].isParsed())
            opt.printSingle("Spider Fusion Rule");
        else if (parser["-interclifford"].isParsed())
            opt.printSingle("Interior Clifford Simp");
        else if (parser["-clifford"].isParsed())
            opt.printSingle("Clifford Simp");
        else
            opt.print();
        return CMD_EXEC_DONE;
    };
    return cmd;
}

//------------------------------------------------------------------------------------------------------------------------------
//    ZXOPTS2s [-Gadgetfusion | -Spiderfusion | -Idremoval | -PIVOTRule | -Lcomp | -PIVOTGadget | -PIVOTBoundary ] <int s2s>
//------------------------------------------------------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXOPTS2sCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXOPTS2s");
    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("set s2s parameter of optimizer for ZXGraph");

        parser.addArgument<int>("s2s")
            .required(true)
            .help("s2s paramenter");

        // auto mutex = parser.addMutuallyExclusiveGroup();

        parser.addArgument<bool>("-idremoval")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform identity removal");

        parser.addArgument<bool>("-lcomp")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform local complementation");

        parser.addArgument<bool>("-gadgetfusion")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform gadget fusion");

        parser.addArgument<bool>("-pivotrule")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform pivot");

        parser.addArgument<bool>("-pivotboundary")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform pivot boundary");

        parser.addArgument<bool>("-pivotgadget")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform pivot gadget");

        parser.addArgument<bool>("-spiderfusion")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform spider fusion");
    };
    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["s2s"].isParsed()) {
            if (parser["-idremoval"].isParsed()) opt.setS2S("Identity Removal Rule", parser["s2s"]);
            if (parser["-lcomp"].isParsed()) opt.setS2S("Local Complementation Rule", parser["s2s"]);
            if (parser["-gadgetfusion"].isParsed()) opt.setS2S("Phase Gadget Rule", parser["s2s"]);
            if (parser["-pivotrule"].isParsed()) opt.setS2S("Pivot Rule", parser["s2s"]);
            if (parser["-pivotboundary"].isParsed()) opt.setS2S("Pivot Boundary Rule", parser["s2s"]);
            if (parser["-pivotgadget"].isParsed()) opt.setS2S("Pivot Gadget Rule", parser["s2s"]);
            if (parser["-spiderfusion"].isParsed()) opt.setS2S("Spider Fusion Rule", parser["s2s"]);
            return CMD_EXEC_DONE;
        } else
            return CMD_EXEC_ERROR;
    };
    return cmd;
}

//------------------------------------------------------------------------------------------------------------------------------
//    ZXOPTR2r [-Gadgetfusion | -Spiderfusion | -Idremoval | -PIVOTRule | -Lcomp | -PIVOTGadget | -PIVOTBoundary ] <int r2r>
//------------------------------------------------------------------------------------------------------------------------------
unique_ptr<ArgParseCmdType> ZXOPTR2rCmd() {
    auto cmd = make_unique<ArgParseCmdType>("ZXOPTR2r");
    cmd->parserDefinition = [](ArgumentParser &parser) {
        parser.help("set r2r parameter of optimizer for ZXGraph");

        parser.addArgument<int>("r2r")
            .required(true)
            .help("r2r paramenter");

        // auto mutex = parser.addMutuallyExclusiveGroup();

        parser.addArgument<bool>("-idremoval")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform identity removal");

        parser.addArgument<bool>("-lcomp")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform local complementation");

        parser.addArgument<bool>("-gadgetfusion")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform gadget fusion");

        parser.addArgument<bool>("-pivotrule")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform pivot");

        parser.addArgument<bool>("-pivotboundary")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform pivot boundary");

        parser.addArgument<bool>("-pivotgadget")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform pivot gadget");

        parser.addArgument<bool>("-spiderfusion")
            .defaultValue(false)
            .action(storeTrue)
            .help("perform spider fusion");

        parser.addArgument<bool>("-interclifford")
            .action(storeTrue)
            .help("perform inter-clifford");

        parser.addArgument<bool>("-clifford")
            .action(storeTrue)
            .help("perform clifford");
    };
    cmd->onParseSuccess = [](ArgumentParser const &parser) {
        if (parser["r2r"].isParsed()) {
            if (parser["-idremoval"].isParsed()) opt.setR2R("Identity Removal Rule", parser["r2r"]);
            if (parser["-lcomp"].isParsed()) opt.setR2R("Local Complementation Rule", parser["r2r"]);
            if (parser["-gadgetfusion"].isParsed()) opt.setR2R("Phase Gadget Rule", parser["r2r"]);
            if (parser["-pivotrule"].isParsed()) opt.setR2R("Pivot Rule", parser["r2r"]);
            if (parser["-pivotboundary"].isParsed()) opt.setR2R("Pivot Boundary Rule", parser["r2r"]);
            if (parser["-pivotgadget"].isParsed()) opt.setR2R("Pivot Gadget Rule", parser["r2r"]);
            if (parser["-spiderfusion"].isParsed()) opt.setR2R("Spider Fusion Rule", parser["r2r"]);
            if (parser["-interclifford"].isParsed()) opt.setR2R("Interior Clifford Simp", parser["r2r"]);
            if (parser["-clifford"].isParsed()) opt.setR2R("Clifford Simp", parser["r2r"]);
            return CMD_EXEC_DONE;
        } else
            return CMD_EXEC_ERROR;
    };
    return cmd;
}
