/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Define global utility functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <gsl/narrow>
#include <iterator>
#include <memory>
#include <string>
#include <tl/generator.hpp>
#include <tl/to.hpp>
#include <utility>
#include <variant>
#include <vector>

class tqdm;  // NOLINT(readability-identifier-naming)  // forward declaration

namespace dvlab {
namespace detail {
void dvlab_assert_impl(std::string_view expr_str, bool expr, std::string_view file, int line, std::string_view msg);
void dvlab_abort_impl(std::string_view file, int line, std::string_view msg);
void dvlab_unreachable_impl(std::string_view file, int line, std::string_view msg);
}  // namespace detail

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

    int idx() const { return _counter; }
    bool done() const { return _counter == _total; }
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
struct overloaded : Ts... {  // NOLINT(readability-identifier-naming)  // mimic library
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename Val, typename... Ts>
auto match(Val&& val, Ts&&... ts) {
    return std::visit(overloaded{std::forward<Ts>(ts)...}, std::forward<Val>(val));
}

auto contains(std::ranges::range auto r, auto const& value) -> bool {
    return std::ranges::find(r, value) != r.end();
}
namespace detail {

template <std::bidirectional_iterator InputIt, typename Compare = std::ranges::less, typename Proj = std::identity>
bool next_combination(const InputIt first, InputIt k, const InputIt last, Compare comp = {}, Proj proj = {}) {
    /* Credits: Mark Nelson http://marknelson.us */
    if ((first == last) || (first == k) || (last == k))
        return false;
    InputIt i1 = first;
    InputIt i2 = last;
    ++i1;
    if (last == i1)
        return false;
    i1 = last;
    --i1;
    i1 = k;
    --i2;
    while (first != i1) {
        if (std::invoke(comp, std::invoke(proj, *--i1), std::invoke(proj, *i2))) {
            InputIt j = k;
            while (!std::invoke(comp, std::invoke(proj, *i1), std::invoke(proj, *j))) ++j;
            std::iter_swap(i1, j);
            ++i1;
            ++j;
            i2 = k;
            std::rotate(i1, j, last);
            while (last != j) {
                ++j;
                ++i2;
            }
            std::rotate(k, i2, last);
            return true;
        }
    }
    std::rotate(first, k, last);
    return false;
}

template <std::ranges::bidirectional_range R, typename Compare = std::ranges::less, typename Proj = std::identity>
auto next_combination(R& r, size_t const comb_size, Compare comp = {}, Proj proj = {}) {
    return detail::next_combination(r.begin(), dvlab::iterator::next(r.begin(), comb_size), r.end(), comp, proj);
}

// iterate over all combinations in the range
template <typename T, typename Compare = std::ranges::less, typename Proj = std::identity>
tl::generator<std::vector<T>> combinations(std::vector<T> elements, size_t const comb_size, Compare comp = {}, Proj proj = {}) {
    std::ranges::sort(elements, comp, proj);
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        auto comb = elements | std::views::take(comb_size) | tl::to<std::vector<T>>();
        co_yield comb;
    } while (next_combination(elements, comb_size, comp, proj));
}

template <typename T, typename Compare = std::ranges::less, typename Proj = std::identity>
tl::generator<std::vector<T>> permutations(std::vector<T> elements, size_t const perm_size, Compare comp = {}, Proj proj = {}) {
    std::ranges::sort(elements, comp, proj);
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        auto perm = elements | std::views::take(perm_size) | tl::to<std::vector<T>>();
        co_yield perm;
    } while (std::ranges::next_permutation(elements, comp, proj));
}

/**
 * @brief generate a tuple type with N elements of type T.
 *
 * @tparam T
 * @tparam N
 * @ref https://stackoverflow.com/a/33513248
 */
template <typename T, size_t N>
class GenerateTupleType {
private:
    template <typename = std::make_index_sequence<N>>
    struct TupleType;

    template <size_t... Is>
    struct TupleType<std::index_sequence<Is...>> {
        template <size_t>
        using wrap = T;
        using type = std::tuple<wrap<Is>...>;
    };

public:
    using type = typename TupleType<>::type;
};

}  // namespace detail

template <typename T, size_t N>
using TupleOfSameType = typename detail::GenerateTupleType<T, N>::type;

template <std::ranges::bidirectional_range R, typename Compare = std::ranges::less, typename Proj = std::identity>
auto next_combination(R& r, size_t const comb_size, Compare comp = {}, Proj proj = {}) {
    return detail::next_combination(r.begin(), dvlab::iterator::next(r.begin(), comb_size), r.end(), comp, proj);
}

// iterate over all combinations in the range
template <typename T, typename Compare = std::ranges::less, typename Proj = std::identity>
tl::generator<std::vector<T>> combinations(std::vector<T> elements, size_t const comb_size, Compare comp = {}, Proj proj = {}) {
    if (elements.size() < comb_size) co_return;
    std::ranges::sort(elements, comp, proj);
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        auto comb = elements | std::views::take(comb_size) | tl::to<std::vector<T>>();
        co_yield comb;
    } while (next_combination(elements, comb_size, comp, proj));
}

template <typename T, typename Compare = std::ranges::less, typename Proj = std::identity>
tl::generator<std::vector<T>> permutations(std::vector<T> elements, size_t const perm_size, Compare comp = {}, Proj proj = {}) {
    if (elements.size() < perm_size) co_return;
    std::ranges::sort(elements, comp, proj);
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        auto perm = elements | std::views::take(perm_size) | tl::to<std::vector<T>>();
        co_yield perm;
    } while (std::ranges::next_permutation(elements, comp, proj));
}

template <size_t N, typename T, typename Compare = std::ranges::less, typename Proj = std::identity>
tl::generator<TupleOfSameType<T, N>> combinations(std::vector<T> elements, Compare comp = {}, Proj proj = {}) {
    if (elements.size() < N) co_return;
    std::ranges::sort(elements, comp, proj);
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        TupleOfSameType<T, N> comb;
        std::apply(
            [&](auto&... comb_elem) {  // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                size_t i = 0;
                ((comb_elem = elements[i++]), ...);
            },
            comb);
        co_yield comb;
    } while (next_combination(elements, N, comp, proj));
}

template <size_t N, typename T, typename Compare = std::ranges::less, typename Proj = std::identity>
tl::generator<TupleOfSameType<T, N>> permutations(std::vector<T> elements, Compare comp = {}, Proj proj = {}) {
    if (elements.size() < N) co_return;
    std::ranges::sort(elements, comp, proj);
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        TupleOfSameType<T, N> perm;
        std::apply(
            [&](auto&... perm_elem) {  // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                size_t i = 0;
                ((perm_elem = elements[i++]), ...);
            },
            perm);
        co_yield perm;
    } while (std::ranges::next_permutation(elements, comp, proj));
}

}  // namespace dvlab

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
/**
 * @brief Asserts that the expression is true.
 *        If NDEBUG is defined, does nothing.
 *        Otherwise, prints a message and aborts the program.
 *
 */
#define DVLAB_ASSERT(Expr, Msg) \
    dvlab::detail::dvlab_assert_impl(#Expr, Expr, __FILE__, __LINE__, Msg)
/**
 * @brief Abort the program with a message.
 *
 */
#define DVLAB_ABORT(Msg) \
    dvlab::detail::dvlab_abort_impl(__FILE__, __LINE__, Msg);

/**
 * @brief Mark a point in the code as unreachable.
 *        If NDEBUG is defined, triggers a compiler-specific hint to the optimizer.
 *        Otherwise, prints a message and aborts the program.
 *
 */
#define DVLAB_UNREACHABLE(Msg) \
    dvlab::detail::dvlab_unreachable_impl(__FILE__, __LINE__, Msg);
// NOLINTEND(cppcoreguidelines-macro-usage)
