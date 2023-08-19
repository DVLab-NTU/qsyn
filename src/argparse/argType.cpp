/****************************************************************************
  FileName     [ argType.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define parser argument types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./argType.hpp"

#include <filesystem>

#include "util/ordered_hashset.hpp"
#include "util/trie.hpp"
#include "util/util.hpp"

using namespace std;

namespace ArgParse {

static_assert(IsContainerType<std::vector<int>> == true);
static_assert(IsContainerType<std::vector<std::string>> == true);
static_assert(IsContainerType<ordered_hashset<float>> == true);
static_assert(IsContainerType<std::string> == false);
static_assert(IsContainerType<std::array<int, 3>> == false);

ArgType<std::string>::ConstraintType choices_allow_prefix(std::vector<std::string> choices) {
    ranges::for_each(choices, [](std::string& str) { str = toLowerString(str); });
    dvlab::utils::Trie trie{choices.begin(), choices.end()};

    auto constraint = [choices, trie](std::string const& val) -> bool {
        return trie.frequency(toLowerString(val)) == 1 ||
               any_of(choices.begin(), choices.end(), [&val, &trie](std::string const& choice) -> bool {
                   return toLowerString(val) == choice;
               });
    };
    auto error = [choices, trie](std::string const& val) -> void {
        if (trie.frequency(val) > 1) {
            fmt::println(stderr, "Error: ambiguous choice \"{}\": could match {}!!\n",
                         val, fmt::join(choices | views::filter([&val](std::string const& choice) { return choice.starts_with(toLowerString(val)); }), " "));
        } else {
            fmt::println(stderr, "Error: ambiguous choice \"{}\": please choose from {{{}}}!!\n",
                         val, fmt::join(choices, " "));
        }
    };

    return {constraint, error};
}

ArgType<std::string>::ConstraintType const path_readable = {
    [](std::string const& filepath) {
        namespace fs = std::filesystem;
        return fs::exists(filepath);
    },
    [](std::string const& filepath) {
        fmt::println(stderr, "Error: the file \"{}\" does not exist!!", filepath);
    }};

ArgType<std::string>::ConstraintType const path_writable = {
    [](std::string const& filepath) {
        namespace fs = std::filesystem;
        auto dir = fs::path{filepath}.parent_path();
        if (dir.empty()) dir = ".";
        return fs::exists(dir);
    },
    [](std::string const& filepath) {
        fmt::println(stderr, "Error: the directory for file \"{}\" does not exist!!", filepath);
    }};

ArgType<std::string>::ConstraintType starts_with(std::vector<std::string> const& prefixes) {
    return {
        [prefixes](std::string const& str) {
            return std::ranges::any_of(prefixes, [&str](std::string const& prefix) { return str.starts_with(prefix); });
        },
        [prefixes](std::string const& str) {
            fmt::println(stderr, "Error: string \"{}\" should start with one of \"{}\"!!",
                         str, fmt::join(prefixes, "\", \""));
        }};
}

ArgType<std::string>::ConstraintType ends_with(std::vector<std::string> const& suffixes) {
    return {
        [suffixes](std::string const& str) {
            return std::ranges::any_of(suffixes, [&str](std::string const& suffix) { return str.ends_with(suffix); });
        },
        [suffixes](std::string const& str) {
            fmt::println(stderr, "Error: string \"{}\" should start end one of \"{}\"!!",
                         str, fmt::join(suffixes, "\", \""));
        }};
}

ArgType<std::string>::ConstraintType allowed_extension(std::vector<std::string> const& extensions) {
    return {
        [extensions](std::string const& str) {
            return std::ranges::any_of(extensions, [&str](std::string const& ext) { return str.substr(std::min(str.find_last_of('.'), str.size())) == ext; });
        },
        [extensions](std::string const& str) {
            fmt::println(stderr, "Error: file must have one of the following extension: \"{}\"!!",
                         fmt::join(extensions, "\", \""));
        }};
}

/**
 * @brief generate a callback that sets the argument to true.
 *        This function also set the default value to false.
 *
 * @param arg
 * @return ArgParse::ActionCallbackType
 */
ActionCallbackType storeTrue(ArgType<bool>& arg) {
    arg.defaultValue(false);
    arg.nargs(0ul);
    return [&arg](TokensView) { arg.appendValue(true); return true; };
}

/**
 * @brief generate a callback that sets the argument to false.
 *        This function also set the default value to true.
 *
 * @param arg
 * @return ArgParse::ActionCallbackType
 */
ActionCallbackType storeFalse(ArgType<bool>& arg) {
    arg.defaultValue(true);
    arg.nargs(0ul);
    return [&arg](TokensView) { arg.appendValue(false); return true; };
}

}  // namespace ArgParse