/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define text formatting functions such as colors, bold, etc. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "./text_format.hpp"

#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <exception>
#include <gsl/narrow>
#include <numeric>
#include <ranges>
#include <tl/to.hpp>

#include "./dvlab_string.hpp"
#include "fmt/color.h"
#include "fmt/core.h"
#include "util/terminal_attributes.hpp"

namespace fs = std::filesystem;

namespace dvlab {

namespace fmt_ext {

constexpr uint8_t ansi_fg_begin        = 30;
constexpr uint8_t ansi_fg_end          = 38;
constexpr uint8_t ansi_bg_begin        = 40;
constexpr uint8_t ansi_bg_end          = 48;
constexpr uint8_t ansi_fg_bright_begin = 90;
constexpr uint8_t ansi_fg_bright_end   = 98;
constexpr uint8_t ansi_bg_bright_begin = 100;
constexpr uint8_t ansi_bg_bright_end   = 108;

namespace {

auto is_ansi_fg_color(uint8_t code) -> bool {
    return (code >= ansi_fg_begin && code < ansi_fg_end) || (code >= ansi_fg_bright_begin && code < ansi_fg_bright_end);
}

auto is_ansi_bg_color(uint8_t code) -> bool {
    return (code >= ansi_bg_begin && code < ansi_bg_end) || (code >= ansi_bg_bright_begin && code < ansi_bg_bright_end);
}

auto const ls_color_map = std::invoke([]() {
    std::unordered_map<std::string, fmt::text_style> map;

    char const* const ls_colors_str = std::getenv("LS_COLORS");
    if (ls_colors_str == nullptr) return map;

    for (auto&& token : dvlab::str::views::split_to_string_views(ls_colors_str, ':')) {
        if (token.empty()) continue;
        auto const equal_sign_pos = token.find('=');

        auto const key        = token.substr(0, equal_sign_pos);
        auto const values_str = token.substr(equal_sign_pos + 1);
        auto const values     = dvlab::str::views::split_to_string_views(values_str, ';') | tl::to<std::vector>();

        auto const style = std::transform_reduce(
            values.begin(), values.end(),
            fmt::text_style{},
            std::bit_or<>{},
            [&](std::string_view str) {
                auto ansi_code = dvlab::str::from_string<uint8_t>(str);
                if (!ansi_code.has_value() || ansi_code == 0) return fmt::text_style{};
                if (is_ansi_fg_color(ansi_code.value())) return fmt::fg(fmt::terminal_color{ansi_code.value()});
                if (is_ansi_bg_color(ansi_code.value())) return fmt::bg(fmt::terminal_color{ansi_code.value()});
                // The shift operator promote the value to an `int`, which is not allowed to initialize a `fmt::emphasis`.
                // Therefore, we need to cast it back to `uint8_t`.
                // If somehow `ansi_code.value()` is not between [0, 7], the narrow cast will panic.
                return fmt::text_style{fmt::emphasis{gsl::narrow<uint8_t>(1 << (ansi_code.value() - 1))}};
            });

        map.emplace(key, style);
    }

    return map;
});

auto ls_color_internal(std::string const& key) -> fmt::text_style {
    if (ls_color_map.contains(key)) return ls_color_map.at(key);

    return fmt::text_style{};
};

}  // namespace

fmt::text_style ls_color(fs::path const& path) {
    namespace fs = std::filesystem;
    using fs::file_type, fs::perms;

    auto const status      = fs::status(path);
    auto const type        = status.type();
    auto const permissions = status.permissions();
    // checks for file types

    switch (type) {
        case file_type::directory: {
            auto const is_sticky         = (permissions & perms::sticky_bit) != perms::none;
            auto const is_other_writable = (permissions & perms::others_write) != perms::none;
            if (is_sticky) {
                return is_other_writable ? ls_color_internal("tw") : ls_color_internal("st");
            }
            return is_other_writable ? ls_color_internal("ow") : ls_color_internal("di");
        }
        case file_type::symlink: {
            return (fs::read_symlink(path) != "")
                       ? ls_color_internal("ln")
                       : ls_color_internal("or");
        }
        // NOTE - omitting multi-hardlinks (mh) -- don't know how to detect it...
        case file_type::fifo:
            return ls_color_internal("pi");
        case file_type::socket:
            return ls_color_internal("so");
        // NOTE - omitting doors (do) -- basically obsolete
        case file_type::block:
            return ls_color_internal("bd");
        case file_type::character:
            return ls_color_internal("cd");
        case file_type::not_found:
            return ls_color_internal("mi");
        default:
            break;
    }

    if ((permissions & perms::set_uid) != perms::none) {
        return ls_color_internal("su");
    }

    if ((permissions & perms::set_gid) != perms::none) {
        return ls_color_internal("sg");
    }

    // NOTE - omitting files with capacities

    // executable files
    if ((permissions & (perms::owner_exec | perms::group_exec | perms::others_exec)) != perms::none) {
        return ls_color_internal("ex");
    }

    std::string ret = std::string("*");
    ret += path.extension().string();
    return ls_color_internal(ret);
}

}  // namespace fmt_ext
}  // namespace dvlab
