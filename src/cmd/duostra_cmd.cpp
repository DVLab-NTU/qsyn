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

#include "cli/cli.hpp"
#include "cmd/device_mgr.hpp"
#include "cmd/qcir_cmd.hpp"
#include "duostra/duostra.hpp"
#include "duostra/duostra_def.hpp"
#include "duostra/layoutcir_mgr.hpp"
#include "duostra/mapping_eqv_checker.hpp"
#include "qcir/qcir.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/text_format.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::duostra {

DuostraConfig DUOSTRA_CONFIG{
    // SECTION - Global settings for Duostra mapper
    .scheduler_type        = SchedulerType::search,
    .router_type           = RouterType::duostra,
    .placer_type           = PlacerType::dfs,
    .tie_breaking_strategy = MinMaxOptionType::min,
    .algorithm_type        = AlgorithmType::subcircuit,

    // SECTION - Initialize in Greedy Scheduler
    .num_candidates          = SIZE_MAX,               // top k candidates, SIZE_MAX: all
    .apsp_coeff              = 1,                      // coefficient of apsp cost
    .available_time_strategy = MinMaxOptionType::max,  // available time of double-qubit gate is set to min or max of occupied time
    .cost_selection_strategy = MinMaxOptionType::min,  // select min or max cost from the waitlist

    // SECTION - Initialize in Search Scheduler
    .search_depth                    = 4,  // depth of searching region
    .never_cache                     = 1,  // never cache any children unless children() is called
    .execute_single_qubit_gates_asap = 0,  // execute the single gates when they are available
};

Command
duostra_config_cmd() {
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
                parser.add_argument<std::string>("--algorithm")
                    .choices({"duostra", "subcircuit"})
                    .help("<duostra | subcircuit>");

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
                    DUOSTRA_CONFIG.scheduler_type = new_scheduler_type.value();
                    printing_config               = false;
                }

                if (parser.parsed("--router")) {
                    auto new_router_type = get_router_type(parser.get<std::string>("--router"));
                    assert(new_router_type.has_value());
                    DUOSTRA_CONFIG.router_type = new_router_type.value();
                    printing_config            = false;
                }

                if (parser.parsed("--placer")) {
                    auto new_placer_type = get_placer_type(parser.get<std::string>("--placer"));
                    assert(new_placer_type.has_value());
                    DUOSTRA_CONFIG.placer_type = new_placer_type.value();
                    printing_config            = false;
                }
                
                if (parser.parsed("--algorithm")) {
                    auto new_algorithm_type = get_algorithm_type(parser.get<std::string>("--algorithm"));
                    assert(new_algorithm_type.has_value());
                    DUOSTRA_CONFIG.algorithm_type = new_algorithm_type.value();
                    printing_config            = false;
                }

                if (parser.parsed("--tie-breaker")) {
                    auto new_tie_breaking_strategy = get_minmax_type(parser.get<std::string>("--tie-breaker"));
                    assert(new_tie_breaking_strategy.has_value());
                    DUOSTRA_CONFIG.tie_breaking_strategy = new_tie_breaking_strategy.value();
                    printing_config                      = false;
                }

                if (parser.parsed("--candidates")) {
                    DUOSTRA_CONFIG.num_candidates = gsl::narrow_cast<size_t>(parser.get<int>("--candidates"));
                    printing_config               = false;
                }

                if (parser.parsed("--apsp-coefficient")) {
                    DUOSTRA_CONFIG.apsp_coeff = parser.get<size_t>("--apsp-coefficient");
                    printing_config           = false;
                }

                if (parser.parsed("--available")) {
                    auto new_available_time_strategy = get_minmax_type(parser.get<std::string>("--available"));
                    assert(new_available_time_strategy.has_value());
                    DUOSTRA_CONFIG.available_time_strategy = new_available_time_strategy.value();
                    printing_config                        = false;
                }

                if (parser.parsed("--cost")) {
                    auto new_cost_selection_strategy = get_minmax_type(parser.get<std::string>("--cost"));
                    assert(new_cost_selection_strategy.has_value());
                    DUOSTRA_CONFIG.cost_selection_strategy = new_cost_selection_strategy.value();
                    printing_config                        = false;
                }

                if (parser.parsed("--depth")) {
                    DUOSTRA_CONFIG.search_depth = (size_t)parser.get<int>("--depth");
                    printing_config             = false;
                }

                if (parser.parsed("--never-cache")) {
                    DUOSTRA_CONFIG.never_cache = parser.get<bool>("--never-cache");
                    printing_config            = false;
                }

                if (parser.parsed("--single-immediately")) {
                    DUOSTRA_CONFIG.execute_single_qubit_gates_asap = parser.get<bool>("--single-immediately");
                    printing_config                                = false;
                }

                if (printing_config) {
                    fmt::println("");
                    fmt::println("Scheduler:         {}", get_scheduler_type_str(DUOSTRA_CONFIG.scheduler_type));
                    fmt::println("Router:            {}", get_router_type_str(DUOSTRA_CONFIG.router_type));
                    fmt::println("Placer:            {}", get_placer_type_str(DUOSTRA_CONFIG.placer_type));
                    fmt::println("Algorithm:         {}", get_algorithm_type_str(DUOSTRA_CONFIG.algorithm_type));

                    if (parser.parsed("--verbose")) {
                        fmt::println("");
                        fmt::println("# Candidates:      {}", ((DUOSTRA_CONFIG.num_candidates == SIZE_MAX) ? "unlimited" : std::to_string(DUOSTRA_CONFIG.num_candidates)));
                        fmt::println("Search Depth:      {}", DUOSTRA_CONFIG.search_depth);
                        fmt::println("");
                        fmt::println("Tie breaker:       {}", get_minmax_type_str(DUOSTRA_CONFIG.tie_breaking_strategy));
                        fmt::println("APSP Coeff.:       {}", DUOSTRA_CONFIG.apsp_coeff);
                        fmt::println("2-Qb. Avail. Time: {}", get_minmax_type_str(DUOSTRA_CONFIG.available_time_strategy));
                        fmt::println("Cost Selector:     {}", get_minmax_type_str(DUOSTRA_CONFIG.cost_selection_strategy));
                        fmt::println("Never Cache:       {}", ((DUOSTRA_CONFIG.never_cache) ? "true" : "false"));
                        fmt::println("Single Immed.:     {}", ((DUOSTRA_CONFIG.execute_single_qubit_gates_asap == 1) ? "true" : "false"));
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
            MappingEquivalenceChecker mpeqc(physical_qc, logical_qc, *device_mgr.get(), DUOSTRA_CONFIG.placer_type, {});
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
                           // ANCHOR - omp disabled
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
                           if (DUOSTRA_CONFIG.algorithm_type == AlgorithmType::subcircuit) {
                                LayoutCirMgr layout_cir_mgr{logical_qcir, *device_mgr.get()};
                            //    spdlog::error("Duostra is not available for subcircuit");
                            //    return CmdExecResult::error;
                           }
                           else{
                                Duostra duo{logical_qcir,
                                        *device_mgr.get(),
                                        DUOSTRA_CONFIG,
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
                           }
                           
                           

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
