/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define the prototypes of the exported utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <concepts>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "tqdm/tqdm.hpp"

class tqdm;

#include <iostream>

#ifndef NDEBUG
namespace dvlab {
namespace detail {
void dvlab_assert_impl(char const* expr_str, bool expr, char const* file, int line, char const* msg);
}
}  // namespace dvlab
#define dvlab_assert(Expr, Msg) \
    dvlab::detail::dvlab_assert_impl(#Expr, Expr, __FILE__, __LINE__, Msg)

#else
#define M_Assert(Expr, Msg) ;
#endif

namespace dvlab {

namespace utils {

bool expect(bool condition, std::string const& msg = "");
size_t int_pow(size_t base, size_t n);

}  // namespace utils

class TqdmWrapper {
public:
    TqdmWrapper(size_t total, bool = true);
    TqdmWrapper(int total, bool = true);
    ~TqdmWrapper();

    size_t idx() const { return _counter; }
    bool done() const { return _counter == _total; }
    void add();
    TqdmWrapper& operator++() {
        add();
        return *this;
    }

private:
    size_t _counter;
    size_t _total;

    // Using a pointer so we don't need to know tqdm's size in advance.
    // This way, no need to #include it in the header
    // because although tqdm works well, it's not expertly written
    // which leads to a lot of warnings.
    std::unique_ptr<tqdm> _tqdm;
};

// In dvlab_string.cpp

namespace str {

template <typename F>
size_t ansi_token_size(F const& fn) {
    return fn("").size();
}

std::optional<std::string> strip_quotes(std::string const& input);
std::string strip_leading_spaces(std::string const& str);
std::string strip_spaces(std::string const& str);
bool is_escaped_char(std::string const& str, size_t pos);
/**
 * @brief strip comment, which starts with "//", from a string
 *
 * @param line
 * @return std::string
 */
inline std::string strip_comments(std::string const& line) { return line.substr(0, line.find("//")); }
std::string remove_brackets(std::string const& str, char const left, char const right);
size_t str_get_token(std::string const& str, std::string& tok, size_t pos = 0, std::string const& delim = " \t\n\v\f\r");
size_t str_get_token(std::string const& str, std::string& tok, size_t pos, char const delim);

std::vector<std::string> split(std::string const& str, std::string const& delim);
std::string join(std::string const& infix, std::span<std::string> strings);

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

std::string to_lower_string(std::string const& str);
std::string to_upper_string(std::string const& str);

}  // namespace str

}  // namespace dvlab

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
T dvlab::str::detail::stonum(std::string const& str, size_t* pos) {
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
            if (dvlab::str::strip_spaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            unsigned long result = std::stoul(str, pos);  // NOTE - for some reason there isn't stou (lol)
            if (result > std::numeric_limits<unsigned>::max()) {
                throw std::out_of_range("stou");
            }
            return (unsigned)result;
        }
        if constexpr (std::is_same<T, unsigned long>::value) {
            if (dvlab::str::strip_spaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
            return std::stoul(str, pos);
        }
        if constexpr (std::is_same<T, unsigned long long>::value) {
            if (dvlab::str::strip_spaces(str)[0] == '-') throw std::out_of_range("unsigned number underflow");
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
bool dvlab::str::str_to_num(std::string const& str, T& f) {
    size_t i;
    try {
        f = dvlab::str::detail::stonum<T>(str, &i);
    } catch (std::exception const& e) {
        return false;
    }
    // Check if str have un-parsable parts
    return (i == str.size());
}
