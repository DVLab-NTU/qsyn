/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <gsl/util>
#include <string>

#include "./duostra.hpp"
#include "./mapping_eqv_checker.hpp"
#include "cli/cli.hpp"
#include "device/device_mgr.hpp"
#include "duostra/duostra_def.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_cmd.hpp"
#include "qcir/qcir_mgr.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

using namespace qsyn::qcir;
using namespace qsyn::device;

namespace qsyn::duostra {

//------------------------------------------------------------------------------
//    DUOSTRA
//------------------------------------------------------------------------------
Command duostra_cmd(QCirMgr& qcir_mgr, DeviceMgr& device_mgr) {
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

            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr) || !device_mgr_not_empty(device_mgr)) return CmdExecResult::error;
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
                qcir::QCir* logical_q_cir = qcir_mgr.get();
                Duostra duo{logical_q_cir, *device_mgr.get(), {.verify_result = parser.get<bool>("-check"), .silent = parser.get<bool>("-silent"), .use_tqdm = !parser.get<bool>("-mute-tqdm")}};
                if (!duo.map()) {
                    return CmdExecResult::error;
                }

                if (duo.get_physical_circuit() == nullptr) {
                    std::cerr << "Error: something wrong in Duostra Mapping!!" << std::endl;
                }
                size_t id = qcir_mgr.get_next_id();
                qcir_mgr.add(id, std::move(duo.get_physical_circuit()));

                qcir_mgr.get()->set_filename(logical_q_cir->get_filename());
                qcir_mgr.get()->add_procedures(logical_q_cir->get_procedures());
                qcir_mgr.get()->add_procedure("Duostra");

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

                parser.add_argument<std::string>("-scheduler")
                    .choices({"base", "naive", "random", "greedy", "search"})
                    .help("<base | naive | random | greedy | search>");
                parser.add_argument<std::string>("-router")
                    .choices({"shortest_path", "duostra"})
                    .help("<shortest_path | duostra>");
                parser.add_argument<std::string>("-placer")
                    .choices({"naive", "random", "dfs"})
                    .help("<naive | random | dfs>");

                parser.add_argument<std::string>("-tie-breaker")
                    .choices({"min", "max"})
                    .help("if tied, execute the operation with the min or max logical qubit index");

                parser.add_argument<int>("-candidates")
                    .help("top k candidates");

                parser.add_argument<size_t>("-apsp-coeff")
                    .help("coefficient of apsp cost");

                parser.add_argument<std::string>("-available")
                    .choices({"min", "max"})
                    .help("available time of double-qubit gate is set to min or max of occupied time");

                parser.add_argument<std::string>("-cost")
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
                if (parser.parsed("-scheduler")) {
                    auto new_scheduler_type = get_scheduler_type(parser.get<std::string>("-scheduler"));
                    assert(new_scheduler_type.has_value());
                    DuostraConfig::SCHEDULER_TYPE = new_scheduler_type.value();
                }

                if (parser.parsed("-router")) {
                    auto new_router_type = get_router_type(parser.get<std::string>("-router"));
                    assert(new_router_type.has_value());
                    DuostraConfig::ROUTER_TYPE = new_router_type.value();
                }

                if (parser.parsed("-placer")) {
                    auto new_placer_type = get_placer_type(parser.get<std::string>("-placer"));
                    assert(new_placer_type.has_value());
                    DuostraConfig::PLACER_TYPE = new_placer_type.value();
                }

                if (parser.parsed("-tie-breaker")) {
                    auto new_tie_breaking_strategy = get_minmax_type(parser.get<std::string>("-tie-breaker"));
                    assert(new_tie_breaking_strategy.has_value());
                    DuostraConfig::TIE_BREAKING_STRATEGY = new_tie_breaking_strategy.value();
                }

                if (parser.parsed("-candidates")) {
                    DuostraConfig::NUM_CANDIDATES = gsl::narrow_cast<size_t>(parser.get<int>("-candidates"));
                }

                if (parser.parsed("-apsp-coeff")) {
                    DuostraConfig::APSP_COEFF = parser.get<size_t>("-apsp-coeff");
                }

                if (parser.parsed("-available")) {
                    auto new_available_time_strategy = get_minmax_type(parser.get<std::string>("-available"));
                    assert(new_available_time_strategy.has_value());
                    DuostraConfig::AVAILABLE_TIME_STRATEGY = new_available_time_strategy.value();
                }

                if (parser.parsed("-cost")) {
                    auto new_cost_selection_strategy = get_minmax_type(parser.get<std::string>("-cost"));
                    assert(new_cost_selection_strategy.has_value());
                    DuostraConfig::COST_SELECTION_STRATEGY = new_cost_selection_strategy.value();
                }

                if (parser.parsed("-depth")) {
                    DuostraConfig::SEARCH_DEPTH = (size_t)parser.get<int>("-depth");
                }

                if (parser.parsed("-never-cache")) {
                    DuostraConfig::NEVER_CACHE = parser.get<bool>("-never-cache");
                }

                if (parser.parsed("-single-immediately")) {
                    DuostraConfig::EXECUTE_SINGLE_QUBIT_GATES_ASAP = parser.get<bool>("-single-immediately");
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
                std::cout << '\n'
                          << "Scheduler:         " << get_scheduler_type_str(DuostraConfig::SCHEDULER_TYPE) << '\n'
                          << "Router:            " << get_router_type_str(DuostraConfig::ROUTER_TYPE) << '\n'
                          << "Placer:            " << get_placer_type_str(DuostraConfig::PLACER_TYPE) << std::endl;

                if (parser.parsed("-detail")) {
                    std::cout << '\n'
                              << "# Candidates:      " << ((DuostraConfig::NUM_CANDIDATES == SIZE_MAX) ? "unlimited" : std::to_string(DuostraConfig::NUM_CANDIDATES)) << '\n'
                              << "Search Depth:      " << DuostraConfig::SEARCH_DEPTH << '\n'
                              << '\n'
                              << "Tie breaker:       " << get_minmax_type_str(DuostraConfig::TIE_BREAKING_STRATEGY) << '\n'
                              << "APSP Coeff.:       " << DuostraConfig::APSP_COEFF << '\n'
                              << "2-Qb. Avail. Time: " << get_minmax_type_str(DuostraConfig::AVAILABLE_TIME_STRATEGY) << '\n'
                              << "Cost Selector:     " << get_minmax_type_str(DuostraConfig::COST_SELECTION_STRATEGY) << '\n'
                              << "Never Cache:       " << ((DuostraConfig::NEVER_CACHE == true) ? "true" : "false") << '\n'
                              << "Single Immed.:     " << ((DuostraConfig::EXECUTE_SINGLE_QUBIT_GATES_ASAP == 1) ? "true" : "false") << std::endl;
                }
                return CmdExecResult::done;
            }};
}

Command mapping_equivalence_check_cmd(qcir::QCirMgr& qcir_mgr, device::DeviceMgr& device_mgr) {
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
        [&](ArgumentParser const& parser) {
            using dvlab::fmt_ext::styled_if_ansi_supported;
            auto physical_qc = qcir_mgr.find_by_id(parser.get<size_t>("-physical"));
            auto logical_qc  = qcir_mgr.find_by_id(parser.get<size_t>("-logical"));
            if (physical_qc == nullptr || logical_qc == nullptr) {
                return CmdExecResult::error;
            }
            MappingEquivalenceChecker mpeqc(physical_qc, logical_qc, *device_mgr.get(), {});
            if (mpeqc.check()) {
                fmt::println("{}", styled_if_ansi_supported("Equivalent up to permutation", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
            } else {
                fmt::println("{}", styled_if_ansi_supported("Not equivalent", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
            }
            return CmdExecResult::done;
        }};
}

bool add_duostra_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr, DeviceMgr& device_mgr) {
    if (!(cli.add_command(duostra_cmd(qcir_mgr, device_mgr)) &&
          cli.add_command(duostra_set_cmd()) &&
          cli.add_command(duostra_print_cmd()) &&
          cli.add_command(mapping_equivalence_check_cmd(qcir_mgr, device_mgr)))) {
        std::cerr << "Registering \"Duostra\" commands fails... exiting" << std::endl;
        return false;
    }
    return true;
}

}  // namespace qsyn::duostra