/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Customized string processing functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <charconv>
#include <concepts>
#include <exception>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <util/util.hpp>
#include <vector>

namespace dvlab::str {

template <typename F>
size_t ansi_token_size(F const& fn) {
    return fn("").size();
}

std::string trim_leading_spaces(std::string_view str);
std::string trim_spaces(std::string_view str);
/**
 * @brief strip comment, which starts with "//", from a string
 *
 * @param line
 * @return std::string
 */
inline std::string_view trim_comments(std::string_view line) { return line.substr(0, line.find("//")); }
std::string remove_brackets(std::string const& str, char const left, char const right);
size_t str_get_token(std::string_view str, std::string& tok, size_t pos = 0, std::string const& delim = " \t\n\v\f\r");
size_t str_get_token(std::string_view str, std::string& tok, size_t pos, char const delim);

namespace detail {
template <class T>
requires std::is_arithmetic_v<T>
T stonum(std::string const& str, size_t* pos);
}

template <class T>
requires std::is_arithmetic_v<T>
bool str_to_num(std::string const& str, T& f);

inline bool str_to_f(std::string const& str, float& num) { return str_to_num<float>(str, num); };
inline bool str_to_d(std::string const& str, double& num) { return str_to_num<double>(str, num); }
inline bool str_to_ld(std::string const& str, long double& num) { return str_to_num<long double>(str, num); }

inline bool str_to_i(std::string const& str, int& num) { return str_to_num<int>(str, num); }
inline bool str_to_l(std::string const& str, long& num) { return str_to_num<long>(str, num); }
inline bool str_to_ll(std::string const& str, long long& num) { return str_to_num<long long>(str, num); }

inline bool str_to_u(std::string const& str, unsigned& num) { return str_to_num<unsigned>(str, num); }
inline bool str_to_ul(std::string const& str, unsigned long& num) { return str_to_num<unsigned long>(str, num); }
inline bool str_to_ull(std::string const& str, unsigned long long& num) { return str_to_num<unsigned long long>(str, num); }

inline bool str_to_size_t(std::string const& str, size_t& num) { return str_to_num<size_t>(str, num); }

char tolower(char ch);
char toupper(char ch);
std::string tolower_string(std::string_view str);
std::string toupper_string(std::string_view str);

namespace views {

/**
 * @brief A wrapper for std::views::split that returns std::string_views instead of std::ranges::subrange so as to ease pipelining.
 *
 * @param str
 * @param delim
 * @return auto
 */
template <typename DelimT>
requires std::convertible_to<DelimT, std::string_view> || std::convertible_to<DelimT, char>
inline auto split_to_string_views(std::string_view str, DelimT delim) {
    return std::views::split(str, delim) | std::views::transform([](auto&& rng) { return std::string_view(&*rng.begin(), std::ranges::distance(rng)); });
}

/**
 * @brief A wrapper for std::views::split that returns std::string_views instead of std::ranges::subrange so as to ease pipelining.
 *
 * @param str
 * @param delim
 * @return auto
 */

/**
 * @brief Split the string into string_views using the given delimiter.
 *
 * @param str The input string to split.
 * @param delim The delimiter to split the string.
 * @return auto The range of string_views after splitting.
 */
inline auto split_to_string_views(std::string_view str, char const* const delim) {
    // Remove the trailing delimiter if present
    auto substring = str.substr(0, str.ends_with(delim) ? str.size() - std::string_view{delim}.size() : str.size());

    // Split the substring into string_views using the delimiter
    return std::views::split(substring, std::string_view{delim}) | std::views::transform([](auto&& range) {
               return std::string_view(&*range.begin(), std::ranges::distance(range));
           });
}

/**
 * @brief skip empty string_views
 *
 */
constexpr auto skip_empty = std::views::filter([](auto&& sv) { return !sv.empty(); });

/**
 * @brief trim leading and trailing spaces
 *
 */
constexpr auto trim_spaces = std::views::transform([](auto&& sv) {
    auto const start = sv.find_first_not_of(" \t\n\v\f\r");
    auto const end   = sv.find_last_not_of(" \t\n\v\f\r");
    if (start == std::string::npos && end == std::string::npos) return std::string_view{""};
    return sv.substr(start, end + 1 - start);
});

/**
 * @brief Tokenize the string with the given delimiter. As opposed to split_to_string_views, this function skips empty tokens and trims spaces.
 *
 * @param str
 * @param delim
 * @return auto
 */
template <typename DelimT>
requires std::convertible_to<DelimT, std::string_view> || std::convertible_to<DelimT, char> || std::convertible_to<DelimT, char const* const>
inline auto tokenize(std::string_view str, DelimT delim) {
    return split_to_string_views(str, delim) | skip_empty | trim_spaces;
}

}  // namespace views
/**
 * @brief An indirection layer for std::stoXXX(const string& str, size_t* pos = nullptr).
 *        All the dirty compile-time checking happens here.
 *
 * @tparam T
 * @param str
 * @param pos
 * @return requires
 */
template <class T>
requires std::is_arithmetic_v<T>
T detail::stonum(std::string const& str, size_t* pos) {
    try {
        // floating point types
        if constexpr (std::is_same<T, double>::value) return std::stod(str, pos);
        if constexpr (std::is_same<T, float>::value) return std::stof(str, pos);
        if constexpr (std::is_same<T, long double>::value) return std::stold(str, pos);

        // signed integer types
        if constexpr (std::is_same<T, int>::value) return std::stoi(str, pos);
        if constexpr (std::is_same<T, long>::value) return std::stol(str, pos);
        if constexpr (std::is_same<T, long long>::value) return std::stoll(str, pos);

        // unsigned integer types
        if constexpr (std::is_same<T, unsigned>::value) {
            if (dvlab::str::trim_spaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            unsigned long const result = std::stoul(str, pos);  // NOTE - for some reason there isn't stou (lol)
            if (result > std::numeric_limits<unsigned>::max()) {
                throw std::out_of_range("stou");
            }
            return (unsigned)result;
        }
        if constexpr (std::is_same<T, unsigned long>::value) {
            if (dvlab::str::trim_spaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            return std::stoul(str, pos);
        }
        if constexpr (std::is_same<T, unsigned long long>::value) {
            if (dvlab::str::trim_spaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            return std::stoull(str, pos);
        }

        throw std::invalid_argument("unsupported type");
    } catch (std::invalid_argument const& e) {
        throw std::invalid_argument(e.what());
    } catch (std::out_of_range const& e) {
        throw std::out_of_range(e.what());
    }

    return 0.;  // silences compiler warnings
}

/**
 * @brief If `str` is a number of type T, parse it and return true;
 *        otherwise, return false.
 *
 * @tparam T
 * @param str
 * @param f
 * @return requires
 */
template <class T>
requires std::is_arithmetic_v<T>
bool str_to_num(std::string const& str, T& f) {
    size_t i = 0;
    try {
        f = detail::stonum<T>(str, &i);
    } catch (std::exception const& e) {
        return false;
    }
    // Check if str have un-parsable parts
    return (i == str.size());
}

namespace detail {

#ifdef _LIBCPP_VERSION
template <typename T>
requires std::integral<T>
std::from_chars_result from_chars_wrapper(std::string_view str, T& val) {
    return std::from_chars(str.data(), str.data() + str.size(), val);
}

template <typename T>
requires std::floating_point<T>
std::from_chars_result from_chars_wrapper(std::string_view str, T& val) {
    size_t pos = 0;
    try {
        if constexpr (std::is_same_v<T, float>) {
            val = std::stof(std::string{str}, &pos);
            return {str.data() + pos, std::errc{}};
        } else if constexpr (std::is_same_v<T, double>) {
            val = std::stod(std::string{str}, &pos);
            return {str.data() + pos, std::errc{}};
        } else if constexpr (std::is_same_v<T, long double>) {
            val = std::stold(std::string{str}, &pos);
            return {str.data() + pos, std::errc{}};
        }
    } catch (std::invalid_argument const& e) {
        return {str.data(), std::errc::invalid_argument};
    } catch (std::out_of_range const& e) {
        return {str.data(), std::errc::result_out_of_range};
    }
    DVLAB_UNREACHABLE("unsupported type");
}

#else
template <typename T>
std::from_chars_result from_chars_wrapper(std::string_view str, T& val) {
    return std::from_chars(str.data(), str.data() + str.size(), val);
}

#endif

}  // namespace detail

template <class T>
inline std::optional<T> from_string(std::string_view str) {
    T result;

    auto [ptr, ec] = detail::from_chars_wrapper<T>(str, result);
    if (ec == std::errc{} && ptr == str.data() + str.size()) {
        return result;
    } else {
        // perror(std::make_error_code(ec).message().c_str());
        return std::nullopt;
    }
}

inline bool is_prefix_of(std::string_view prefix, std::string_view str) {
    return str.starts_with(prefix);
}

}  // namespace dvlab::str
