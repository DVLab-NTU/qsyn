/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define text formatting functions such as colors, bold, etc. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "./text_format.hpp"

#include <unistd.h>

#include <cassert>
#include <exception>
#include <ranges>

#include "./util.hpp"
#include "fmt/color.h"
#include "fmt/core.h"
#include "util/terminal_attributes.hpp"

namespace fs = std::filesystem;

namespace dvlab {

namespace fmt_ext {

fmt::text_style ls_color(fs::path const& path) {
    static auto const ls_color_internal = [](std::string const& key) -> fmt::text_style {
        static auto const ls_color_map = std::invoke([]() {
            std::unordered_map<std::string, fmt::text_style> map;
            char const* const ls_colors_str = getenv("LS_COLORS");
            if (ls_colors_str == nullptr) return map;
            std::string ls_colors{ls_colors_str};

            for (auto&& token : dvlab::str::split(ls_colors, ":")) {
                if (token.empty()) continue;
                size_t pos                      = token.find('=');
                std::string key                 = token.substr(0, pos);
                std::vector<std::string> values = dvlab::str::split(token.substr(pos + 1), ";");
                fmt::text_style style           = std::transform_reduce(
                    values.begin(), values.end(),
                    fmt::text_style{},
                    std::bit_or<>{},
                    [&](std::string const& str) {
                        size_t tmp;
                        if (!dvlab::str::str_to_num<size_t>(str, tmp) || tmp == 0) return fmt::text_style{};
                        if (tmp >= 30 && tmp <= 37) return fmt::fg(fmt::terminal_color{gsl::narrow<uint8_t>(tmp)});
                        if (tmp >= 90 && tmp <= 97) return fmt::fg(fmt::terminal_color{gsl::narrow<uint8_t>(tmp)});
                        if (tmp >= 40 && tmp <= 47) return fmt::bg(fmt::terminal_color{gsl::narrow<uint8_t>(tmp)});
                        if (tmp >= 100 && tmp <= 107) return fmt::bg(fmt::terminal_color{gsl::narrow<uint8_t>(tmp)});
                        assert(tmp >= 0 && tmp <= 7);
                        return fmt::text_style{fmt::emphasis{gsl::narrow<uint8_t>(1 << (tmp - 1))}};
                    });

                map.emplace(key, style);
            }

            return map;
        });

        if (ls_color_map.contains(key)) return ls_color_map.at(key);

        return fmt::text_style{};
    };

    namespace fs = std::filesystem;
    using fs::file_type, fs::perms;

    auto status      = fs::status(path);
    auto type        = status.type();
    auto permissions = status.permissions();
    // checks for file types

    switch (type) {
        case file_type::directory: {
            bool is_sticky         = (permissions & perms::sticky_bit) != perms::none;
            bool is_other_writable = (permissions & perms::others_write) != perms::none;
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

    return ls_color_internal("*" + path.extension().string());
}

}  // namespace fmt_ext
}  // namespace dvlab
