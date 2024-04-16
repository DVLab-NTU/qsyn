/****************************************************************************
  PackageName  [ duostra ]
  Synopsis     [ Define Duostra package commands ]
  Author       [ Chin-Yi Cheng, Chien-Yi Yang, Ren-Chu Wang, Yi-Hsiang Kuo ]
  Paper        [ https://arxiv.org/abs/2210.01306 ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>

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
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/text_format.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::duostra {

Command duostra_config_cmd() {
    return {"config",
            [](ArgumentParser& parser) {
                parser.description("set Duostra parameter(s)");

                parser.add_argument<std::string>("--scheduler")
                    .choices({"base", "naive", "random", "greedy", "search"})
                    .help("<base | naive | random | greedy | search>");
                parser.add_argument<std::string>("--router")
                    .choices({"shortest_path", "duostra"})
                    .help("<shortest_path | duostra>");
                parser.add_argument<std::string>("--placer")
                    .choices({"naive", "random", "dfs"})
                    .help("<naive | random | dfs>");

                parser.add_argument<std::string>("--tie-breaker")
                    .choices({"min", "max"})
                    .help("if tied, execute the operation with the min or max logical qubit index");

                parser.add_argument<int>("--candidates")
                    .help("top k candidates");

                parser.add_argument<size_t>("--apsp-coefficient")
                    .help("coefficient of apsp cost");

                parser.add_argument<std::string>("--available")
                    .choices({"min", "max"})
                    .help("available time of double-qubit gate is set to min or max of occupied time");

                parser.add_argument<std::string>("--cost")
                    .choices({"min", "max"})
                    .help("select min or max cost from the waitlist");

                parser.add_argument<int>("--depth")
                    .help("depth of searching region");

                parser.add_argument<bool>("--never-cache")
                    .help("never cache any children unless children() is called");

                parser.add_argument<bool>("--single-immediately")
                    .help("execute the single gates when they are available");

                parser.add_argument<bool>("-v", "--verbose")
                    .help("print detailed information. This option only has effect when other options are not set")
                    .action(store_true);
            },

            [](ArgumentParser const& parser) {
                auto printing_config = true;
                if (parser.parsed("--scheduler")) {
                    auto new_scheduler_type = get_scheduler_type(parser.get<std::string>("--scheduler"));
                    assert(new_scheduler_type.has_value());
                    DuostraConfig::SCHEDULER_TYPE = new_scheduler_type.value();
                    printing_config               = false;
                }

                if (parser.parsed("--router")) {
                    auto new_router_type = get_router_type(parser.get<std::string>("--router"));
                    assert(new_router_type.has_value());
                    DuostraConfig::ROUTER_TYPE = new_router_type.value();
                    printing_config            = false;
                }

                if (parser.parsed("--placer")) {
                    auto new_placer_type = get_placer_type(parser.get<std::string>("--placer"));
                    assert(new_placer_type.has_value());
                    DuostraConfig::PLACER_TYPE = new_placer_type.value();
                    printing_config            = false;
                }

                if (parser.parsed("--tie-breaker")) {
                    auto new_tie_breaking_strategy = get_minmax_type(parser.get<std::string>("--tie-breaker"));
                    assert(new_tie_breaking_strategy.has_value());
                    DuostraConfig::TIE_BREAKING_STRATEGY = new_tie_breaking_strategy.value();
                    printing_config                      = false;
                }

                if (parser.parsed("--candidates")) {
                    DuostraConfig::NUM_CANDIDATES = gsl::narrow_cast<size_t>(parser.get<int>("--candidates"));
                    printing_config               = false;
                }

                if (parser.parsed("--apsp-coefficient")) {
                    DuostraConfig::APSP_COEFF = parser.get<size_t>("--apsp-coefficient");
                    printing_config           = false;
                }

                if (parser.parsed("--available")) {
                    auto new_available_time_strategy = get_minmax_type(parser.get<std::string>("--available"));
                    assert(new_available_time_strategy.has_value());
                    DuostraConfig::AVAILABLE_TIME_STRATEGY = new_available_time_strategy.value();
                    printing_config                        = false;
                }

                if (parser.parsed("--cost")) {
                    auto new_cost_selection_strategy = get_minmax_type(parser.get<std::string>("--cost"));
                    assert(new_cost_selection_strategy.has_value());
                    DuostraConfig::COST_SELECTION_STRATEGY = new_cost_selection_strategy.value();
                    printing_config                        = false;
                }

                if (parser.parsed("--depth")) {
                    DuostraConfig::SEARCH_DEPTH = (size_t)parser.get<int>("--depth");
                    printing_config             = false;
                }

                if (parser.parsed("--never-cache")) {
                    DuostraConfig::NEVER_CACHE = parser.get<bool>("--never-cache");
                    printing_config            = false;
                }

                if (parser.parsed("--single-immediately")) {
                    DuostraConfig::EXECUTE_SINGLE_QUBIT_GATES_ASAP = parser.get<bool>("--single-immediately");
                    printing_config                                = false;
                }

                if (printing_config) {
                    fmt::println("");
                    fmt::println("Scheduler:         {}", get_scheduler_type_str(DuostraConfig::SCHEDULER_TYPE));
                    fmt::println("Router:            {}", get_router_type_str(DuostraConfig::ROUTER_TYPE));
                    fmt::println("Placer:            {}", get_placer_type_str(DuostraConfig::PLACER_TYPE));

                    if (parser.parsed("--verbose")) {
                        fmt::println("");
                        fmt::println("# Candidates:      {}", ((DuostraConfig::NUM_CANDIDATES == SIZE_MAX) ? "unlimited" : std::to_string(DuostraConfig::NUM_CANDIDATES)));
                        fmt::println("Search Depth:      {}", DuostraConfig::SEARCH_DEPTH);
                        fmt::println("");
                        fmt::println("Tie breaker:       {}", get_minmax_type_str(DuostraConfig::TIE_BREAKING_STRATEGY));
                        fmt::println("APSP Coeff.:       {}", DuostraConfig::APSP_COEFF);
                        fmt::println("2-Qb. Avail. Time: {}", get_minmax_type_str(DuostraConfig::AVAILABLE_TIME_STRATEGY));
                        fmt::println("Cost Selector:     {}", get_minmax_type_str(DuostraConfig::COST_SELECTION_STRATEGY));
                        fmt::println("Never Cache:       {}", ((DuostraConfig::NEVER_CACHE) ? "true" : "false"));
                        fmt::println("Single Immed.:     {}", ((DuostraConfig::EXECUTE_SINGLE_QUBIT_GATES_ASAP == 1) ? "true" : "false"));
                    }
                }

                return CmdExecResult::done;
            }};
}

Command mapping_equivalence_check_cmd(qcir::QCirMgr& qcir_mgr, device::DeviceMgr& device_mgr) {
    return {
        "map-equiv",
        [](ArgumentParser& parser) {
            parser.description("check equivalence of the physical and the logical circuits");
            parser.add_argument<size_t>("-l", "--logical")
                .metavar("l-id")
                .required(true)
                .help("the ID to the logical QCir");
            parser.add_argument<size_t>("-p", "--physical")
                .metavar("p-id")
                .required(true)
                .help("the ID to the physical QCir");
            parser.add_argument<bool>("-r", "--reverse")
                .default_value(false)
                .action(store_true)
                .help("check the QCir in reverse. This option is supposed to be used for extracted QCir");
        },
        [&](ArgumentParser const& parser) {
            using dvlab::fmt_ext::styled_if_ansi_supported;
            auto physical_qc = qcir_mgr.find_by_id(parser.get<size_t>("--physical"));
            auto logical_qc  = qcir_mgr.find_by_id(parser.get<size_t>("--logical"));
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

Command duostra_cmd(qcir::QCirMgr& qcir_mgr, device::DeviceMgr& device_mgr) {
    auto cmd = Command{"duostra",
                       [](ArgumentParser& parser) {
                           parser.description("map logical circuit to physical circuit");
                           parser.add_argument<bool>("-c", "--check")
                               .default_value(false)
                               .action(store_true)
                               .help("check whether the mapping result is correct");
                           parser.add_argument<bool>("--mute-tqdm")
                               .default_value(false)
                               .action(store_true)
                               .help("mute tqdm");
                           parser.add_argument<bool>("-s", "--silent")
                               .default_value(false)
                               .action(store_true)
                               .help("mute all messages");
                       },

                       [&](ArgumentParser const& parser) {
                           if (!dvlab::utils::mgr_has_data(qcir_mgr) || !dvlab::utils::mgr_has_data(device_mgr)) return CmdExecResult::error;
// #ifdef __GNUC__
//                            char const* const omp_wait_policy = std::getenv("OMP_WAIT_POLICY");

//                            if (omp_wait_policy == nullptr || (strcasecmp(omp_wait_policy, "passive") != 0)) {
//                                spdlog::error("Cannot run command `DUOSTRA`: environment variable `OMP_WAIT_POLICY` is not set to `PASSIVE`.");
//                                spdlog::error("Note: Not setting the `OMP_WAIT_POLICY` to `PASSIVE` may cause the program to freeze.");
//                                spdlog::error("      You can set it to PASSIVE by running `export OMP_WAIT_POLICY=PASSIVE`");
//                                spdlog::error("      prior to running `qsyn`.");
//                                return CmdExecResult::error;
//                            }
// #endif
                           qcir::QCir* logical_qcir = qcir_mgr.get();
                           Duostra duo{logical_qcir,
                                       *device_mgr.get(),
                                       {.verify_result = parser.get<bool>("--check"),
                                        .silent        = parser.get<bool>("--silent"),
                                        .use_tqdm      = !parser.get<bool>("--mute-tqdm")}};
                           if (!duo.map()) {
                               return CmdExecResult::error;
                           }

                           if (duo.get_physical_circuit() == nullptr) {
                               spdlog::error("Detected error in Duostra Mapping!!");
                           }
                           auto const id = qcir_mgr.get_next_id();
                           qcir_mgr.add(id, std::move(duo.get_physical_circuit()));

                           qcir_mgr.get()->set_filename(logical_qcir->get_filename());
                           qcir_mgr.get()->add_procedures(logical_qcir->get_procedures());
                           qcir_mgr.get()->add_procedure("Duostra");

                           return CmdExecResult::done;
                       }};

    cmd.add_subcommand("duostra-cmd", duostra_config_cmd());
    return cmd;
}

bool add_duostra_cmds(dvlab::CommandLineInterface& cli, qcir::QCirMgr& qcir_mgr, device::DeviceMgr& device_mgr) {
    if (!(cli.add_command(duostra_cmd(qcir_mgr, device_mgr)) &&
          cli.add_command(mapping_equivalence_check_cmd(qcir_mgr, device_mgr)))) {
        spdlog::error("Registering \"Duostra\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn::duostra
