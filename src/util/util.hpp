/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define global utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <concepts>
#include <gsl/narrow>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class tqdm;

namespace dvlab {
#ifndef NDEBUG
namespace detail {
void dvlab_assert_impl(std::string_view expr_str, bool expr, std::string_view file, int line, std::string_view msg);
void dvlab_abort_impl(std::string_view file, int line, std::string_view msg);
void dvlab_unreachable_impl(std::string_view file, int line, std::string_view msg);
}  // namespace detail
#endif

namespace utils {

bool expect(bool condition, std::string const& msg = "");

/**
 * @brief Specialization of std::pow where both the base and the exponent are non-negative integers.
 *
 * @param base
 * @param n
 * @return constexpr size_t
 */
constexpr size_t int_pow(size_t base, size_t exponent) {
    if (exponent == 0) return 1;
    if (exponent == 1) return base;
    auto const tmp = int_pow(base, exponent / 2);
    if (exponent % 2 == 0)
        return tmp * tmp;
    else
        return base * tmp * tmp;
}

}  // namespace utils

class TqdmWrapper {
public:
    using CounterType = int;
    TqdmWrapper(size_t total, bool show = true);
    ~TqdmWrapper();

    TqdmWrapper(TqdmWrapper const&)                = delete;
    TqdmWrapper& operator=(TqdmWrapper const&)     = delete;
    TqdmWrapper(TqdmWrapper&&) noexcept            = default;
    TqdmWrapper& operator=(TqdmWrapper&&) noexcept = default;

    inline int idx() const { return _counter; }
    inline bool done() const { return _counter == _total; }
    TqdmWrapper& operator++();

private:
    CounterType _counter = 0;
    CounterType _total;

    // Using a pointer so we don't need to know tqdm's size in advance.
    // This way, no need to #include it in the header
    // because although tqdm works well, it's not expertly written
    // which leads to a lot of warnings.
    std::unique_ptr<tqdm> _tqdm;
};

// In dvlab_string.cpp

namespace iterator {

/**
 * @brief A wrapper for std::next() that throws an exception if the narrowing conversion fails.
 *
 * @tparam Iter iterator type. Requires std::random_access_iterator.
 * @tparam DiffT difference type. Requires std::integral.
 */
template <typename Iter, typename DiffT>
requires std::random_access_iterator<Iter> && std::integral<DiffT>
Iter next(Iter iter, DiffT n = 1) {
    return std::next(iter, gsl::narrow<typename decltype(iter)::difference_type>(n));
}

/**
 * @brief A wrapper for std::prev() that throws an exception if the narrowing conversion fails.
 *
 * @tparam Iter iterator type. Requires std::random_access_iterator.
 * @tparam DiffT difference type. Requires std::integral.
 */
template <typename Iter, typename DiffT>
requires std::random_access_iterator<Iter> && std::integral<DiffT>
Iter prev(Iter iter, DiffT n = 1) {
    return std::prev(iter, gsl::narrow<typename decltype(iter)::difference_type>(n));
}

}  // namespace iterator

// [std::visit - cppreference.com](https://en.cppreference.com/w/cpp/utility/variant/visit)
// helper type for the visitor

/**
 * @brief helps to define a visitor for std::visit. For example,
          std::visit(overloaded(<lambda function for each variant type>), visitee);
 *
 * @tparam Ts
 */
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace dvlab

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define DVLAB_ASSERT(Expr, Msg) \
    dvlab::detail::dvlab_assert_impl(#Expr, Expr, __FILE__, __LINE__, Msg)
#define DVLAB_ABORT(Msg)                                      \
    dvlab::detail::dvlab_abort_impl(__FILE__, __LINE__, Msg); \
    abort()
#define DVLAB_UNREACHABLE(Msg)                                      \
    dvlab::detail::dvlab_unreachable_impl(__FILE__, __LINE__, Msg); \
    abort()
// NOLINTEND(cppcoreguidelines-macro-usage)
