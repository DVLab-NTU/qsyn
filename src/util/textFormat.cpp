/****************************************************************************
  FileName     [ textFormat.cpp ]
  PackageName  [ util ]
  Synopsis     [ Define text formatting functions such as colors, bold, etc. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "./textFormat.hpp"

#include <exception>
#include <filesystem>

#include "./util.hpp"

extern size_t colorLevel;

namespace TextFormat {

std::string decorate(std::string const& str, const std::string& code) {
    return "\033[" + code + "m" + str + "\033[0m";
}

std::string decorate(std::string const& str, const size_t& code) {
    return decorate(str, std::to_string(code));
}

std::string setFormat(const std::string& str, const size_t& code) {
    return (colorLevel >= 1) ? decorate(str, code) : str;
}

size_t tokenSize(std::function<std::string(std::string const&)> F) {
    return F("").size();
}

std::string BOLD(const std::string& str) { return setFormat(str, 1); }
std::string DIM(const std::string& str) { return setFormat(str, 2); }
std::string ITALIC(const std::string& str) { return setFormat(str, 3); }
std::string ULINE(const std::string& str) { return setFormat(str, 4); }
std::string SWAP(const std::string& str) { return setFormat(str, 7); }
std::string STRIKE(const std::string& str) { return setFormat(str, 9); }

std::string BLACK(const std::string& str) { return setFormat(str, 30); }
std::string RED(const std::string& str) { return setFormat(str, 31); }
std::string GREEN(const std::string& str) { return setFormat(str, 32); }
std::string YELLOW(const std::string& str) { return setFormat(str, 33); }
std::string BLUE(const std::string& str) { return setFormat(str, 34); }
std::string MAGENTA(const std::string& str) { return setFormat(str, 35); }
std::string CYAN(const std::string& str) { return setFormat(str, 36); }
std::string WHITE(const std::string& str) { return setFormat(str, 37); }

std::string BG_BLACK(const std::string& str) { return setFormat(str, 40); }
std::string BG_RED(const std::string& str) { return setFormat(str, 41); }
std::string BG_GREEN(const std::string& str) { return setFormat(str, 42); }
std::string BG_YELLOW(const std::string& str) { return setFormat(str, 43); }
std::string BG_BLUE(const std::string& str) { return setFormat(str, 44); }
std::string BG_MAGENTA(const std::string& str) { return setFormat(str, 45); }
std::string BG_CYAN(const std::string& str) { return setFormat(str, 46); }
std::string BG_WHITE(const std::string& str) { return setFormat(str, 47); }

std::string GRAY(const std::string& str) { return setFormat(str, 90); }
std::string LIGHT_RED(const std::string& str) { return setFormat(str, 91); }
std::string LIGHT_GREEN(const std::string& str) { return setFormat(str, 92); }
std::string LIGHT_YELLOW(const std::string& str) { return setFormat(str, 93); }
std::string LIGHT_BLUE(const std::string& str) { return setFormat(str, 94); }
std::string LIGHT_MAGENTA(const std::string& str) { return setFormat(str, 95); }
std::string LIGHT_CYAN(const std::string& str) { return setFormat(str, 96); }
std::string LIGHT_WHITE(const std::string& str) { return setFormat(str, 97); }

std::string BG_GRAY(const std::string& str) { return setFormat(str, 100); }
std::string LIGHT_BG_RED(const std::string& str) { return setFormat(str, 101); }
std::string LIGHT_BG_GREEN(const std::string& str) { return setFormat(str, 102); }
std::string LIGHT_BG_YELLOW(const std::string& str) { return setFormat(str, 103); }
std::string LIGHT_BG_BLUE(const std::string& str) { return setFormat(str, 104); }
std::string LIGHT_BG_MAGENTA(const std::string& str) { return setFormat(str, 105); }
std::string LIGHT_BG_CYAN(const std::string& str) { return setFormat(str, 106); }
std::string LIGHT_BG_WHITE(const std::string& str) { return setFormat(str, 107); }

std::string LS_COLOR(const std::string& basename, std::string const& dirname) {
    static auto const ls_color = [](std::string const& key) -> std::string {
        static auto ls_color_map = std::invoke([]() {
            std::unordered_map<std::string, std::string> map;
            std::string ls_colors{getenv("LS_COLORS")};

            for (auto&& token : split(ls_colors, ":")) {
                if (token.empty()) continue;
                size_t pos = token.find('=');
                map.emplace(token.substr(0, pos), token.substr(pos + 1));
            }

            return map;
        });

        if (ls_color_map.contains(key)) return ls_color_map.at(key);

        return "0";
    };

    namespace fs = std::filesystem;
    using fs::file_type, fs::perms;

    auto status = fs::status(fs::path(dirname) / fs::path(basename));
    auto type = status.type();
    auto permissions = status.permissions();
    // checks for file types

    switch (type) {
        case file_type::directory: {
            bool isSticky = (permissions & perms::sticky_bit) != perms::none;
            bool isOtherWritable = (permissions & perms::others_write) != perms::none;
            if (isSticky) {
                if (isOtherWritable) {
                    return decorate(basename, ls_color("tw"));
                }
                return decorate(basename, ls_color("st"));
            }

            if (isOtherWritable) {
                return decorate(basename, ls_color("ow"));
            }
            return decorate(basename, ls_color("di"));
        }
        case file_type::symlink: {
            return (fs::read_symlink(basename) != "")
                       ? decorate(basename, ls_color("ln"))
                       : decorate(basename, ls_color("or"));
        }
        // NOTE - omitting multi-hardlinks (mh) -- don't know how to detect it...
        case file_type::fifo:
            return decorate(basename, ls_color("pi"));
        case file_type::socket:
            return decorate(basename, ls_color("so"));
        // NOTE - omitting doors (do) -- basically obsolete
        case file_type::block:
            return decorate(basename, ls_color("bd"));
        case file_type::character:
            return decorate(basename, ls_color("cd"));
        case file_type::not_found:
            return decorate(basename, ls_color("mi"));
        default:
            break;
    }

    if ((permissions & perms::set_uid) != perms::none) {
        return decorate(basename, ls_color("su"));
    }

    if ((permissions & perms::set_gid) != perms::none) {
        return decorate(basename, ls_color("sg"));
    }

    // NOTE - omitting files with capacities

    // executable files
    if ((permissions & (perms::owner_exec | perms::group_exec | perms::others_exec)) != perms::none) {
        return decorate(basename, ls_color("ex"));
    }

    return decorate(basename, ls_color("*" + fs::path(basename).extension().string()));
}
};  // namespace TextFormat