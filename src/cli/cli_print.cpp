/****************************************************************************
  PackageName  [ cli ]
  Synopsis     [ Define printing functions of class dvlab::CommandLineInterface ]
  Author       [ Design Verification Lab, Chia-Hsu Chuang ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

#include <ranges>

#include "./cli.hpp"
#include "tl/enumerate.hpp"
#include "tl/to.hpp"

namespace dvlab {

/**
 * @brief print a summary of all commands
 *
 */
void dvlab::CommandLineInterface::list_all_commands() const {
    auto cmd_vec = _commands | std::views::keys | tl::to<std::vector>();
    std::ranges::sort(cmd_vec);
    for (auto const& cmd : cmd_vec)
        _commands.at(cmd)->print_summary();
}

/**
 * @brief print a summary of all aliases
 *
 */
void dvlab::CommandLineInterface::list_all_aliases() const {
    std::vector<std::pair<std::string, std::string>> alias_vec(std::begin(_aliases), std::end(_aliases));
    std::ranges::sort(alias_vec);
    for (auto const& [alias, cmd] : alias_vec)
        fmt::println("{:>10} = \"{}\"", alias, cmd);

    fmt::println("");
}

void dvlab::CommandLineInterface::list_all_variables() const {
    std::vector<std::pair<std::string, std::string>> var_vec(std::begin(_variables), std::end(_variables));
    std::ranges::sort(var_vec);
    for (auto const& [var, val] : var_vec)
        fmt::println("{:>10} = {}", var, val);

    fmt::println("");
}

/**
 * @brief print the last nPrint commands in CLI history
 *
 * @param nPrint
 */
void dvlab::CommandLineInterface::print_history(size_t n_print, HistoryFilter filter) const {
    assert(_temp_command_stored == false);

    if (n_print > _history.size())
        n_print = _history.size();

    if (_history.empty()) {
        fmt::println(("Empty command history!!"));
        return;
    }
    auto hist_range = _history |
                      std::views::drop(_history.size() - n_print) |
                      tl::views::enumerate |
                      std::views::filter([&filter](auto const& history) {
                          return (filter.success && history.second.status == CmdExecResult::done) ||
                                 (filter.error && history.second.status == CmdExecResult::error) ||
                                 (filter.unknown && history.second.status == CmdExecResult::cmd_not_found) ||
                                 (filter.interrupted && history.second.status == CmdExecResult::interrupted);
                      });
    for (auto const& [i, history] : hist_range) {
        fmt::println("{:>4}: {}", i, history.input);
    }
}

void dvlab::CommandLineInterface::write_history(std::filesystem::path const& filepath, size_t n_print, bool append_quit, HistoryFilter filter) const {
    assert(_temp_command_stored == false);

    if (n_print > _history.size())
        n_print = _history.size();

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        spdlog::error("Cannot open file: {}\n", filepath.string());
        return;
    }
    if (_history.empty()) {
        spdlog::info("Empty command history!!");
        return;
    }
    auto hist_range = _history |
                      std::views::drop(_history.size() - n_print) |
                      std::views::filter([&filter](auto const& history) {
                          return (filter.success && history.status == CmdExecResult::done) ||
                                 (filter.error && history.status == CmdExecResult::error) ||
                                 (filter.unknown && history.status == CmdExecResult::cmd_not_found) ||
                                 (filter.interrupted && history.status == CmdExecResult::interrupted);
                      });
    for (auto const& history : hist_range) {
        fmt::println(ofs, "{}", history.input);
    }
    if (append_quit) {
        fmt::println(ofs, "quit -f");
    }
}

}  // namespace dvlab
