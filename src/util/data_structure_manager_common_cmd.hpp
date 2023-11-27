/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define data structure manager template ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./data_structure_manager.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "util/dvlab_string.hpp"

namespace dvlab::utils {

template <typename T>
requires manager_manageable<T>
std::function<bool(size_t const&)> valid_mgr_id(DataStructureManager<T>& mgr) {
    return [&mgr](size_t const& id) -> bool {
        if (mgr.is_id(id)) return true;
        spdlog::error("{} {} does not exist!!", mgr.get_type_name(), id);
        return false;
    };
}

template <typename T>
requires manager_manageable<T>
bool mgr_has_data(DataStructureManager<T> const& mgr) {
    if (mgr.empty()) {
        spdlog::error("{0} list is empty. Please create a {0} first!!", mgr.get_type_name());
        return false;
    }
    return true;
}

template <typename T>
requires manager_manageable<T>
Command mgr_root_cmd(DataStructureManager<T>& mgr) {
    using namespace dvlab::argparse;
    return {
        dvlab::str::tolower_string(mgr.get_type_name()),
        [&](ArgumentParser& parser) {
            parser.description(fmt::format("{} commands", mgr.get_type_name()));
        },
        [&](ArgumentParser const& /* parser */) {
            mgr.print_manager();
            return CmdExecResult::done;
        }};
}

template <typename T>
requires manager_manageable<T>
Command mgr_list_cmd(DataStructureManager<T>& mgr) {
    using namespace dvlab::argparse;
    return {"list",
            [&](ArgumentParser& parser) {
                parser.description(fmt::format("List all {}s", mgr.get_type_name()));
            },
            [&](ArgumentParser const& /* parser */) {
                mgr.print_list();
                return CmdExecResult::done;
            }};
}

template <typename T>
requires manager_manageable<T>
Command mgr_checkout_cmd(DataStructureManager<T>& mgr) {
    using dvlab::argparse::ArgumentParser;
    return {"checkout",
            [&](ArgumentParser& parser) {
                parser.description(fmt::format("Checkout to {} with the ID specified", mgr.get_type_name()));
                parser.add_argument<size_t>("id")
                    .constraint(valid_mgr_id(mgr))
                    .help(fmt::format("the ID of the {}", mgr.get_type_name()));
            },
            [&](ArgumentParser const& parser) {
                if (!mgr_has_data(mgr)) return CmdExecResult::error;
                mgr.checkout(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

template <typename T>
requires manager_manageable<T>
Command mgr_new_cmd(DataStructureManager<T>& mgr) {
    using namespace dvlab::argparse;
    return {"new",
            [&](ArgumentParser& parser) {
                parser.description(fmt::format("Create a new {}", mgr.get_type_name()));

                parser.add_argument<size_t>("id")
                    .nargs(NArgsOption::optional)
                    .help(fmt::format("the ID of the {}", mgr.get_type_name()));

                parser.add_argument<bool>("-r", "--replace")
                    .action(store_true)
                    .help(fmt::format("if specified, replace the current {}; otherwise create a new one", mgr.get_type_name()));
            },
            [&](ArgumentParser const& parser) {
                auto id = parser.parsed("id") ? parser.get<size_t>("id") : mgr.get_next_id();

                if (mgr.is_id(id)) {
                    if (!parser.parsed("--replace")) {
                        spdlog::error("{} {} already exists!! Please specify `--replace` to replace if needed", mgr.get_type_name(), id);
                        return CmdExecResult::error;
                    }
                    mgr.set_by_id(id, std::make_unique<T>());
                    return CmdExecResult::done;
                }

                mgr.add(id);

                return CmdExecResult::done;
            }};
}

template <typename T>
requires manager_manageable<T>
Command mgr_delete_cmd(DataStructureManager<T>& mgr) {
    using namespace dvlab::argparse;
    return {"delete",
            [&](ArgumentParser& parser) {
                parser.description(fmt::format("Delete a {} from the list", mgr.get_type_name(), mgr.get_type_name()));

                auto mutex = parser.add_mutually_exclusive_group();

                mutex.add_argument<size_t>("id")
                    .nargs(NArgsOption::optional)
                    .constraint(valid_mgr_id(mgr))
                    .help(fmt::format("the ID of the {}", mgr.get_type_name()));

                mutex.add_argument<bool>("--all")
                    .action(store_true)
                    .help(fmt::format("delete all {}s", mgr.get_type_name()));
            },
            [&](ArgumentParser const& parser) {
                if (!mgr_has_data(mgr)) return CmdExecResult::error;

                if (parser.parsed("--all")) {
                    mgr.clear();
                } else {
                    mgr.remove(parser.get<size_t>("id"));
                }

                return CmdExecResult::done;
            }};
}

template <typename T>
requires manager_manageable<T>
Command mgr_copy_cmd(DataStructureManager<T>& mgr) {
    using namespace dvlab::argparse;
    return {"copy",
            [&](ArgumentParser& parser) {
                parser.description(fmt::format("Copy a {}", mgr.get_type_name()));

                parser.add_argument<size_t>("id")
                    .nargs(NArgsOption::optional)
                    .help(fmt::format("the ID of the new {}", mgr.get_type_name()));

                parser.add_argument<bool>("-r", "--replace")
                    .action(store_true)
                    .help(fmt::format("replace the current {} if there is one", mgr.get_type_name()));
            },
            [&](ArgumentParser const& parser) {
                if (!mgr_has_data(mgr)) return CmdExecResult::error;
                auto id = parser.parsed("id") ? parser.get<size_t>("id") : mgr.get_next_id();

                if (mgr.is_id(id)) {
                    if (!parser.parsed("--replace")) {
                        spdlog::error("{} {} already exists!! Please specify `--replace` to replace it.", mgr.get_type_name(), id);
                        return CmdExecResult::error;
                    }
                }
                mgr.copy(id);
                return CmdExecResult::done;
            }};
}

}  // namespace dvlab::utils
