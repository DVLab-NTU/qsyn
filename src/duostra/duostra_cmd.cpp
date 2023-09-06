/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <string>

#include "./duostra.hpp"
#include "./mapping_eqv_checker.hpp"
#include "./variables.hpp"
#include "cli/cli.hpp"
#include "device/device_mgr.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_cmd.hpp"
#include "qcir/qcir_mgr.hpp"

using namespace std;
using namespace argparse;
extern QCirMgr QCIR_MGR;
extern DeviceMgr DEVICE_MGR;

Command duostra_cmd();
Command duostra_print_cmd();
Command duostra_set_cmd();
Command mapping_equivalence_check_cmd();

bool add_duostra_cmds() {
    if (!(CLI.add_command(duostra_cmd()) &&
          CLI.add_command(duostra_set_cmd()) &&
          CLI.add_command(duostra_print_cmd()) &&
          CLI.add_command(mapping_equivalence_check_cmd()))) {
        cerr << "Registering \"Duostra\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
//    DUOSTRA
//------------------------------------------------------------------------------
Command duostra_cmd() {
    return {"duostra",
            [](ArgumentParser& parser) {
                parser.description("map logical circuit to physical circuit");
                parser.add_argument<bool>("-check")
                    .default_value(false)
                    .action(store_true)
                    .help("check whether the mapping result is correct");
                parser.add_argument<bool>("-mute-tqdm")
                    .default_value(false)
                    .action(store_true)
                    .help("mute tqdm");
                parser.add_argument<bool>("-silent")
                    .default_value(false)
                    .action(store_true)
                    .help("mute all messages");
            },

            [](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty() || !device_mgr_not_empty()) return CmdExecResult::error;
#ifdef __GNUC__
                char const* const omp_wait_policy = getenv("OMP_WAIT_POLICY");

                if (omp_wait_policy == nullptr || (strcasecmp(omp_wait_policy, "passive") != 0)) {
                    LOGGER.error("Cannot run command `DUOSTRA`: OMP_WAIT_POLICY is not set to PASSIVE.");
                    LOGGER.error("Note: Not setting the above environmental variable may cause the program to freeze.");
                    LOGGER.error("      You can set it to PASSIVE by running `export OMP_WAIT_POLICY=PASSIVE`");
                    LOGGER.error("      prior to running `qsyn`.");
                    return CmdExecResult::error;
                }
#endif
                QCir* logical_q_cir = QCIR_MGR.get();
                Duostra duo{logical_q_cir, *DEVICE_MGR.get(), {.verifyResult = parser.get<bool>("-check"), .silent = parser.get<bool>("-silent"), .useTqdm = !parser.get<bool>("-mute-tqdm")}};
                if (duo.flow() == SIZE_MAX) {
                    return CmdExecResult::error;
                }

                if (duo.get_physical_circuit() == nullptr) {
                    cerr << "Error: something wrong in Duostra Mapping!!" << endl;
                }
                size_t id = QCIR_MGR.get_next_id();
                QCIR_MGR.add(id, std::move(duo.get_physical_circuit()));

                QCIR_MGR.get()->set_filename(logical_q_cir->get_filename());
                QCIR_MGR.get()->add_procedures(logical_q_cir->get_procedures());
                QCIR_MGR.get()->add_procedure("Duostra");

                return CmdExecResult::done;
            }};
}

//------------------------------------------------------------------------------
//    DUOSET  ..... neglect
//------------------------------------------------------------------------------
Command duostra_set_cmd() {
    return {"duoset",
            [](ArgumentParser& parser) {
                parser.description("set Duostra parameter(s)");

                parser.add_argument<string>("-scheduler")
                    .choices({"base", "static", "random", "greedy", "search"})
                    .help("< base   | static | random | greedy | search >");
                parser.add_argument<string>("-router")
                    .choices({"apsp", "duostra"})
                    .help("< apsp   | duostra >");
                parser.add_argument<string>("-placer")
                    .choices({"static", "random", "dfs"})
                    .help("< static | random | dfs >");

                parser.add_argument<bool>("-orient")
                    .help("smaller logical qubit index with little priority");

                parser.add_argument<int>("-candidates")
                    .help("top k candidates");

                parser.add_argument<size_t>("-apsp-coeff")
                    .help("coefficient of apsp cost");

                parser.add_argument<string>("-available")
                    .choices({"min", "max"})
                    .help("available time of double-qubit gate is set to min or max of occupied time");

                parser.add_argument<string>("-cost")
                    .choices({"min", "max"})
                    .help("select min or max cost from the waitlist");

                parser.add_argument<int>("-depth")
                    .help("depth of searching region");

                parser.add_argument<bool>("-never-cache")
                    .help("never cache any children unless children() is called");

                parser.add_argument<bool>("-single-immediately")
                    .help("execute the single gates when they are available");
            },

            [](ArgumentParser const& parser) {
                if (parser.parsed("-scheduler"))
                    DUOSTRA_SCHEDULER = get_scheduler_type(parser.get<string>("-scheduler"));
                if (parser.parsed("-router"))
                    DUOSTRA_ROUTER = get_router_type(parser.get<string>("-router"));
                if (parser.parsed("-placer"))
                    DUOSTRA_PLACER = get_placer_type(parser.get<string>("-placer"));
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

                return CmdExecResult::done;
            }};
}

//------------------------------------------------------------------------------
//    DUOPrint [-detail]
//------------------------------------------------------------------------------
Command duostra_print_cmd() {
    return {"duoprint",
            [](ArgumentParser& parser) {
                parser.description("print Duostra parameters");
                parser.add_argument<bool>("-detail")
                    .default_value(false)
                    .action(store_true)
                    .help("print detailed information");
            },
            [](ArgumentParser const& parser) {
                cout << '\n'
                     << "Scheduler:         " << get_scheduler_type_str() << '\n'
                     << "Router:            " << get_router_type_str() << '\n'
                     << "Placer:            " << get_placer_type_str() << endl;

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
                return CmdExecResult::done;
            }};
}

Command mapping_equivalence_check_cmd() {
    return {
        "mpequiv",
        [](ArgumentParser& parser) {
            parser.description("check equivalence of the physical and the logical circuits");
            parser.add_argument<size_t>("-logical")
                .required(true)
                .help("logical circuit id");
            parser.add_argument<size_t>("-physical")
                .required(true)
                .help("physical circuit id");
            parser.add_argument<bool>("-reverse")
                .default_value(false)
                .action(store_true)
                .help("check the circuit reversily, used in extracted circuit");
        },
        [](ArgumentParser const& parser) {
            using namespace dvlab;
            auto physical_qc = QCIR_MGR.find_by_id(parser.get<size_t>("-physical"));
            auto logical_qc = QCIR_MGR.find_by_id(parser.get<size_t>("-logical"));
            if (physical_qc == nullptr || logical_qc == nullptr) {
                return CmdExecResult::error;
            }
            MappingEQChecker mpeqc(physical_qc, logical_qc, *DEVICE_MGR.get(), {});
            if (mpeqc.check()) {
                fmt::println("{}", fmt_ext::styled_if_ansi_supported("Equivalent up to permutation", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
            } else {
                fmt::println("{}", fmt_ext::styled_if_ansi_supported("Not equivalent", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
            }
            return CmdExecResult::done;
        }};
}
