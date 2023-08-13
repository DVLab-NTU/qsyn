/****************************************************************************
  FileName     [ argType.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define parser argument types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./argType.hpp"

#include <filesystem>
#include <iostream>

#include "util/ordered_hashset.hpp"
#include "util/trie.hpp"
#include "util/util.hpp"

using namespace std;

namespace ArgParse {

namespace detail {

std::ostream& _cout = cout;
std::ostream& _cerr = cerr;

}  // namespace detail

static_assert(IsContainerType<std::vector<int>> == true);
static_assert(IsContainerType<std::vector<std::string>> == true);
static_assert(IsContainerType<ordered_hashset<float>> == true);
static_assert(IsContainerType<std::string> == false);
static_assert(IsContainerType<std::array<int, 3>> == false);

std::ostream& operator<<(std::ostream& os, DummyArgType const& val) { return os << "dummy"; }

template <>
std::string typeString(int) { return "int"; }
template <>
std::string typeString(long) { return "long"; }
template <>
std::string typeString(long long) { return "long long"; }

template <>
std::string typeString(unsigned) { return "unsigned"; }
template <>
std::string typeString(unsigned long) { return "size_t"; }
template <>
std::string typeString(unsigned long long) { return "size_t"; }

template <>
std::string typeString(float) { return "float"; }
template <>
std::string typeString(double) { return "double"; }
template <>
std::string typeString(long double) { return "long double"; }

std::string typeString(std::string const& val) { return "string"; }
std::string typeString(bool) { return "bool"; }
std::string typeString(DummyArgType) { return "dummy"; }

bool parseFromString(bool& val, std::string const& token) {
    if ("true"s.starts_with(toLowerString(token))) {
        val = true;
        return true;
    } else if ("false"s.starts_with(toLowerString(token))) {
        val = false;
        return true;
    }
    return false;
}

bool parseFromString(std::string& val, std::string const& token) {
    val = token;
    return true;
}

bool parseFromString(DummyArgType& val, std::string const& token) {
    return true;
}

ArgType<std::string>::ConstraintType choices_allow_prefix(std::vector<std::string> choices) {
    ranges::for_each(choices, [](std::string& str) { str = toLowerString(str); });
    dvlab_utils::Trie trie{choices.begin(), choices.end()};

    auto constraint = [choices, trie](std::string const& val) -> bool {
        return trie.frequency(toLowerString(val)) == 1 ||
               any_of(choices.begin(), choices.end(), [&val, &trie](std::string const& choice) -> bool {
                   return toLowerString(val) == choice;
               });
    };
    auto error = [choices, trie](std::string const& val) -> void {
        if (trie.frequency(val) > 1) {
            std::cerr << "Error: ambiguous choice \"" << val << "\": could match";

            for (auto& choice : choices) {
                if (choice.starts_with(toLowerString(val))) std::cerr << " " << choice;
            }
            std::cerr << "!!\n";
            return;
        } else {
            std::cerr << "Error: invalid choice \"" << val << "\": please choose from {";
            size_t ctr = 0;
            for (auto& choice : choices) {
                if (ctr > 0) std::cerr << ", ";
                std::cerr << choice;
                ctr++;
            }
            std::cerr << "}!!\n";
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
        std::cerr << "Error: the file \"" << filepath << "\" does not exist!!" << std::endl;
    }};

ArgType<std::string>::ConstraintType const path_writable = {
    [](std::string const& filepath) {
        size_t lastSlash = filepath.find_last_of('/');
        if (lastSlash == std::string::npos) return std::filesystem::exists(".");
        return std::filesystem::exists(filepath.substr(0, lastSlash));
    },
    [](std::string const& filepath) {
        std::cerr << "Error: the directory for file \"" << filepath << "\" does not exist!!" << std::endl;
    }};

ArgType<std::string>::ConstraintType starts_with(std::vector<std::string> const& prefixes) {
    return {
        [prefixes](std::string const& str) {
            return std::ranges::any_of(prefixes, [&str](std::string const& prefix) { return str.starts_with(prefix); });
        },
        [prefixes](std::string const& str) {
            std::cerr << "Error: string \"" << str << "\" should start with one of";
            for (auto& prefix : prefixes) {
                std::cerr << " \"" << prefix << "\"";
            }
            std::cerr << "!!" << std::endl;
        }};
}

ArgType<std::string>::ConstraintType ends_with(std::vector<std::string> const& suffixes) {
    return {
        [suffixes](std::string const& str) {
            return std::ranges::any_of(suffixes, [&str](std::string const& suffix) { return str.ends_with(suffix); });
        },
        [suffixes](std::string const& str) {
            std::cerr << "Error: string \"" << str << "\" should end with one of";
            for (auto& suffix : suffixes) {
                std::cerr << " \"" << suffix << "\"";
            }
            std::cerr << "!!" << std::endl;
        }};
}

ArgType<std::string>::ConstraintType allowed_extension(std::vector<std::string> const& extensions) {
    return {
        [extensions](std::string const& str) {
            return std::ranges::any_of(extensions, [&str](std::string const& ext) { return str.substr(std::min(str.find_last_of('.'), str.size())) == ext; });
        },
        [extensions](std::string const& str) {
            std::cerr << "Error: file must have one of the following extension:";
            for (auto& ext : extensions) {
                std::cerr << " \"" << ext << "\"";
            }
            std::cerr << "!!" << std::endl;
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