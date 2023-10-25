/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define argparse arg types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./arg_type.hpp"

#include <filesystem>

#include "util/ordered_hashset.hpp"
#include "util/trie.hpp"

namespace dvlab::argparse {

static_assert(is_container_type<std::vector<int>> == true);
static_assert(is_container_type<std::vector<std::string>> == true);
static_assert(is_container_type<dvlab::utils::ordered_hashset<float>> == true);
static_assert(is_container_type<std::string> == false);
static_assert(is_container_type<std::array<int, 3>> == false);

ArgType<std::string>::ConstraintType choices_allow_prefix(std::vector<std::string> choices) {
    std::ranges::for_each(choices, [](std::string& str) { str = dvlab::str::tolower_string(str); });
    dvlab::utils::Trie const trie{std::begin(choices), std::end(choices)};

    return [choices, trie](std::string const& val) -> bool {
        auto const is_exact_match_to_choice = [&val](std::string const& choice) -> bool {
            return dvlab::str::tolower_string(val) == choice;
        };
        auto const freq = trie.frequency(dvlab::str::tolower_string(val));

        if (freq == 1) return true;
        if (std::ranges::any_of(choices, is_exact_match_to_choice)) return true;

        if (freq > 1) {
            fmt::println(stderr, "Error: ambiguous choice \"{}\": could match {}!!\n",
                         val, fmt::join(choices | std::views::filter([&val](std::string const& choice) { return choice.starts_with(dvlab::str::tolower_string(val)); }), ", "));
        } else {
            fmt::println(stderr, "Error: invalid choice \"{}\": please choose from {{{}}}!!\n",
                         val, fmt::join(choices, ", "));
        }
        return false;
    };
}

bool path_readable(std::string const& filepath) {
    namespace fs = std::filesystem;
    if (!fs::exists(filepath)) {
        fmt::println(stderr, "Error: the file \"{}\" does not exist!!", filepath);
        return false;
    }
    return true;
}

bool path_writable(std::string const& filepath) {
    namespace fs = std::filesystem;
    auto dir     = fs::path{filepath}.parent_path();
    if (dir.empty()) dir = ".";
    if (!fs::exists(dir)) {
        fmt::println(stderr, "Error: the directory for file \"{}\" does not exist!!", filepath);
        return false;
    }
    return true;
}

ArgType<std::string>::ConstraintType starts_with(std::vector<std::string> const& prefixes) {
    return [prefixes](std::string const& str) {
        if (std::ranges::none_of(prefixes, [&str](std::string const& prefix) { return str.starts_with(prefix); })) {
            fmt::println(stderr, "Error: string \"{}\" should start with one of \"{}\"!!",
                         str, fmt::join(prefixes, "\", \""));
            return false;
        }
        return true;
    };
}

ArgType<std::string>::ConstraintType ends_with(std::vector<std::string> const& suffixes) {
    return [suffixes](std::string const& str) {
        if (std::ranges::none_of(suffixes, [&str](std::string const& suffix) { return str.ends_with(suffix); })) {
            fmt::println(stderr, "Error: string \"{}\" should start end one of \"{}\"!!",
                         str, fmt::join(suffixes, "\", \""));
            return false;
        }
        return true;
    };
}

ArgType<std::string>::ConstraintType allowed_extension(std::vector<std::string> const& extensions) {
    return [extensions](std::string const& str) {
        if (std::ranges::none_of(extensions, [&str](std::string const& ext) { return str.substr(std::min(str.find_last_of('.'), str.size())) == ext; })) {
            fmt::println(stderr, "Error: file \"{}\" must have one of the following extension: \"{}\"!!",
                         str, fmt::join(extensions, "\", \""));
            return false;
        }
        return true;
    };
}

/**
 * @brief generate a callback that sets the argument to true.
 *        This function also set the default value to false.
 *
 * @param arg
 * @return argparse::ActionCallbackType
 */
ActionCallbackType store_true(ArgType<bool>& arg) {
    arg.default_value(false);
    arg.nargs(0ul);
    return [&arg](TokensView) { arg.append_value(true); return true; };
}

/**
 * @brief generate a callback that sets the argument to false.
 *        This function also set the default value to true.
 *
 * @param arg
 * @return argparse::ActionCallbackType
 */
ActionCallbackType store_false(ArgType<bool>& arg) {
    arg.default_value(true);
    arg.nargs(0ul);
    return [&arg](TokensView) { arg.append_value(false); return true; };
}

ActionCallbackType help(ArgType<bool>& arg) {
    arg.mark_as_help_action();
    arg.nargs(0ul);
    return [](TokensView) -> bool {
        return true;
    };
}

ActionCallbackType version(ArgType<bool>& arg) {
    arg.mark_as_version_action();
    arg.nargs(0ul);
    return [](TokensView) -> bool {
        return true;
    };
}

}  // namespace dvlab::argparse
