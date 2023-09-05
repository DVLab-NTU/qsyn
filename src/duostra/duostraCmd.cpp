/****************************************************************************
  FileName     [ duostraCmd.cpp ]
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <string>

#include "./duostra.hpp"
#include "./mappingEQChecker.hpp"
#include "./variables.hpp"
#include "cli/cli.hpp"
#include "device/deviceMgr.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcirCmd.hpp"
#include "qcir/qcirMgr.hpp"

using namespace std;
using namespace ArgParse;
extern QCirMgr qcirMgr;
extern DeviceMgr deviceMgr;

Command duostraCmd();
Command duostraPrintCmd();
Command duostraSetCmd();
Command mapEQCmd();

bool initDuostraCmd() {
    if (!(cli.registerCommand(duostraCmd()) &&
          cli.registerCommand(duostraSetCmd()) &&
          cli.registerCommand(duostraPrintCmd()) &&
          cli.registerCommand(mapEQCmd()))) {
        cerr << "Registering \"Duostra\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
//    DUOSTRA
//------------------------------------------------------------------------------
Command duostraCmd() {
    return {"duostra",
            [](ArgumentParser& parser) {
                parser.description("map logical circuit to physical circuit");
                parser.addArgument<bool>("-check")
                    .defaultValue(false)
                    .action(storeTrue)
                    .help("check whether the mapping result is correct");
                parser.addArgument<bool>("-mute-tqdm")
                    .defaultValue(false)
                    .action(storeTrue)
                    .help("mute tqdm");
                parser.addArgument<bool>("-silent")
                    .defaultValue(false)
                    .action(storeTrue)
                    .help("mute all messages");
            },

            [](ArgumentParser const& parser) {
                if (!qcirMgrNotEmpty() || !deviceMgrNotEmpty()) return CmdExecResult::ERROR;
#ifdef __GNUC__
                char const* const ompWaitPolicy = getenv("OMP_WAIT_POLICY");

                if (ompWaitPolicy == nullptr || (strcasecmp(ompWaitPolicy, "passive") != 0)) {
                    logger.error("Cannot run command `DUOSTRA`: OMP_WAIT_POLICY is not set to PASSIVE.");
                    logger.error("Note: Not setting the above environmental variable may cause the program to freeze.");
                    logger.error("      You can set it to PASSIVE by running `export OMP_WAIT_POLICY=PASSIVE`");
                    logger.error("      prior to running `qsyn`.");
                    return CmdExecResult::ERROR;
                }
#endif
                QCir* logicalQCir = qcirMgr.get();
                Duostra duo{logicalQCir, *deviceMgr.get(), {.verifyResult = parser.get<bool>("-check"), .silent = parser.get<bool>("-silent"), .useTqdm = !parser.get<bool>("-mute-tqdm")}};
                if (duo.flow() == ERROR_CODE) {
                    return CmdExecResult::ERROR;
                }

                if (duo.getPhysicalCircuit() == nullptr) {
                    cerr << "Error: something wrong in Duostra Mapping!!" << endl;
                }
                size_t id = qcirMgr.getNextID();
                qcirMgr.add(id, std::move(duo.getPhysicalCircuit()));

                qcirMgr.get()->setFileName(logicalQCir->getFileName());
                qcirMgr.get()->addProcedures(logicalQCir->getProcedures());
                qcirMgr.get()->addProcedure("Duostra");

                return CmdExecResult::DONE;
            }};
}

//------------------------------------------------------------------------------
//    DUOSET  ..... neglect
//------------------------------------------------------------------------------
Command duostraSetCmd() {
    return {"duoset",
            [](ArgumentParser& parser) {
                parser.description("set Duostra parameter(s)");

                parser.addArgument<string>("-scheduler")
                    .choices({"base", "static", "random", "greedy", "search"})
                    .help("< base   | static | random | greedy | search >");
                parser.addArgument<string>("-router")
                    .choices({"apsp", "duostra"})
                    .help("< apsp   | duostra >");
                parser.addArgument<string>("-placer")
                    .choices({"static", "random", "dfs"})
                    .help("< static | random | dfs >");

                parser.addArgument<bool>("-orient")
                    .help("smaller logical qubit index with little priority");

                parser.addArgument<int>("-candidates")
                    .help("top k candidates");

                parser.addArgument<size_t>("-apsp-coeff")
                    .help("coefficient of apsp cost");

                parser.addArgument<string>("-available")
                    .choices({"min", "max"})
                    .help("available time of double-qubit gate is set to min or max of occupied time");

                parser.addArgument<string>("-cost")
                    .choices({"min", "max"})
                    .help("select min or max cost from the waitlist");

                parser.addArgument<int>("-depth")
                    .help("depth of searching region");

                parser.addArgument<bool>("-never-cache")
                    .help("never cache any children unless children() is called");

                parser.addArgument<bool>("-single-immediately")
                    .help("execute the single gates when they are available");
            },

            [](ArgumentParser const& parser) {
                if (parser.parsed("-scheduler"))
                    DUOSTRA_SCHEDULER = getSchedulerType(parser.get<string>("-scheduler"));
                if (parser.parsed("-router"))
                    DUOSTRA_ROUTER = getRouterType(parser.get<string>("-router"));
                if (parser.parsed("-placer"))
                    DUOSTRA_PLACER = getPlacerType(parser.get<string>("-placer"));
                if (parser.parsed("-orient"))
                    DUOSTRA_ORIENT = parser.get<bool>("-orient");

                if (parser.parsed("-candidates")) {
                    DUOSTRA_CANDIDATES = (size_t)parser.get<int>("-candidates");
                }

                if (parser.parsed("-apsp-coeff")) {
                    DUOSTRA_APSP_COEFF = parser.get<size_t>("-apsp-coeff");
                }

                if (parser.parsed("-available")) {
                    DUOSTRA_AVAILABLE = (parser.get<string>("-available") == "min") ? false : true;
                }

                if (parser.parsed("-cost")) {
                    DUOSTRA_COST = (parser.get<string>("-cost") == "min") ? false : true;
                }

                if (parser.parsed("-depth")) {
                    DUOSTRA_DEPTH = (size_t)parser.get<int>("-depth");
                }

                if (parser.parsed("-never-cache")) {
                    DUOSTRA_NEVER_CACHE = parser.get<bool>("-never-cache");
                }

                if (parser.parsed("-single-immediately")) {
                    DUOSTRA_EXECUTE_SINGLE = parser.get<bool>("-single-immediately");
                }

                return CmdExecResult::DONE;
            }};
}

//------------------------------------------------------------------------------
//    DUOPrint [-detail]
//------------------------------------------------------------------------------
Command duostraPrintCmd() {
    return {"duoprint",
            [](ArgumentParser& parser) {
                parser.description("print Duostra parameters");
                parser.addArgument<bool>("-detail")
                    .defaultValue(false)
                    .action(storeTrue)
                    .help("print detailed information");
            },
            [](ArgumentParser const& parser) {
                cout << '\n'
                     << "Scheduler:         " << getSchedulerTypeStr() << '\n'
                     << "Router:            " << getRouterTypeStr() << '\n'
                     << "Placer:            " << getPlacerTypeStr() << endl;

                if (parser.parsed("-detail")) {
                    cout << '\n'
                         << "Candidates:        " << ((DUOSTRA_CANDIDATES == size_t(-1)) ? "-1" : to_string(DUOSTRA_CANDIDATES)) << '\n'
                         << "Search Depth:      " << DUOSTRA_DEPTH << '\n'
                         << '\n'
                         << "Orient:            " << ((DUOSTRA_ORIENT == 1) ? "true" : "false") << '\n'
                         << "APSP Coeff.:       " << DUOSTRA_APSP_COEFF << '\n'
                         << "Available Time:    " << ((DUOSTRA_AVAILABLE == 0) ? "min" : "max") << '\n'
                         << "Prefer Cost:       " << ((DUOSTRA_COST == 0) ? "min" : "max") << '\n'
                         << "Never Cache:       " << ((DUOSTRA_NEVER_CACHE == 1) ? "true" : "false") << '\n'
                         << "Single Immed.:     " << ((DUOSTRA_EXECUTE_SINGLE == 1) ? "true" : "false") << endl;
                }
                return CmdExecResult::DONE;
            }};
}

Command mapEQCmd() {
    return {
        "mpequiv",
        [](ArgumentParser& parser) {
            parser.description("check equivalence of the physical and the logical circuits");
            parser.addArgument<size_t>("-logical")
                .required(true)
                .help("logical circuit id");
            parser.addArgument<size_t>("-physical")
                .required(true)
                .help("physical circuit id");
            parser.addArgument<bool>("-reverse")
                .defaultValue(false)
                .action(storeTrue)
                .help("check the circuit reversily, used in extracted circuit");
        },
        [](ArgumentParser const& parser) {
            using namespace dvlab;
            auto physicalQC = qcirMgr.findByID(parser.get<size_t>("-physical"));
            auto logicalQC = qcirMgr.findByID(parser.get<size_t>("-logical"));
            if (physicalQC == nullptr || logicalQC == nullptr) {
                return CmdExecResult::ERROR;
            }
            MappingEQChecker mpeqc(physicalQC, logicalQC, *deviceMgr.get(), {});
            if (mpeqc.check()) {
                fmt::println("{}", fmt_ext::styled_if_ANSI_supported("Equivalent up to permutation", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
            } else {
                fmt::println("{}", fmt_ext::styled_if_ANSI_supported("Not equivalent", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
            }
            return CmdExecResult::DONE;
        }};
}
