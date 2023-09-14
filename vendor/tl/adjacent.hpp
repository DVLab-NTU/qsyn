#ifndef TL_RANGES_ADJACENT_HPP
#define TL_RANGES_ADJACENT_HPP

#include <ranges>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"
#include "utility/meta.hpp"
#include "utility/tuple_utils.hpp"

namespace tl {
    template<std::ranges::forward_range V, std::size_t N>
        requires std::ranges::view<V> && (N > 0)
    class adjacent_view : public std::ranges::view_interface<adjacent_view<V, N>> {
        V base_{};

        template<bool Const>
        class cursor {
            using Base = maybe_const<Const, V>;
            using Diff = std::ranges::range_difference_t<Base>;

        public:
            // invariant: ranges::distance(base_) < N and first_ == last_ == ranges::end(base_), or
            // ranges::distance(base_) >= N and next(first_, N - 1) == last_
            std::ranges::iterator_t<Base> first_{}, last_{};
            
            using value_type = typename tl::meta::repeat_into<std::ranges::range_value_t<Base>, N, detail::tuple_or_pair_impl>::type;

            using difference_type = std::ranges::range_difference_t<Base>;

            cursor() = default;
            constexpr cursor(cursor<!Const> i)
                requires Const && std::convertible_to<std::ranges::iterator_t<V>, std::ranges::iterator_t<Base>> :
                first_(std::move(i.first_)),
                last_(std::move(i.last_))
            {}

            constexpr cursor(std::ranges::iterator_t<Base> first, std::ranges::sentinel_t<Base> const last) :
                first_(first),
                last_(std::move(first))
            {
                std::ranges::advance(last_, Diff{N} - 1, last);
                if (last_ == last) {
                    first_ = last_;
                }
            }
            constexpr cursor(as_sentinel_t, std::ranges::iterator_t<Base> first, std::ranges::iterator_t<Base> last) :
                first_(last),
                last_(std::move(last))
            {
                if constexpr (std::ranges::bidirectional_range<Base>) {
                    if (auto remainder = std::ranges::advance(first_, -Diff{N} + 1, first)) {
                        first_ = last_;
                    }
                }
            }

            constexpr auto read() const {
                return [i = first_]<std::size_t... Indices>(std::index_sequence<Indices...>) mutable {
                    using Ref = tl::meta::repeat_into<
                        std::ranges::range_reference_t<Base>, N, detail::tuple_or_pair_impl>::type;
                    return Ref{((void) Indices, *i++)...};
                }(std::make_index_sequence<N>{});
            }

            constexpr void next() {
                ++first_;
                ++last_;
            }

            constexpr void prev() requires std::ranges::bidirectional_range<Base> {
                --first_;
                --last_;
            }

            constexpr void advance(difference_type x) requires std::ranges::random_access_range<Base> {
                first_ += x;
                last_ += x;
            }

            constexpr bool equal(cursor const& rhs) const
                requires std::equality_comparable<std::ranges::iterator_t<Base>>{
                return last_ == rhs.last_;
            }

            constexpr bool equal(basic_sentinel<V, Const> const& rhs) const {
                return last_ == rhs.end_;
            }

            constexpr auto distance_to(const cursor& rhs) const
                requires std::sized_sentinel_for<std::ranges::iterator_t<Base>, std::ranges::iterator_t<Base>> {
                return rhs.last_ - last_;
            }

            constexpr auto distance_to(const basic_sentinel<V, Const>& rhs) const
                requires std::sized_sentinel_for<std::ranges::sentinel_t<Base>, std::ranges::iterator_t<Base>> {
                return rhs.end_ - last_;
            }
        };

    public:
        constexpr adjacent_view() requires std::default_initializable<V> = default;
        constexpr explicit adjacent_view(V base) : base_(std::move(base)) {}

        constexpr auto begin() requires (!tl::simple_view<V>) {
            return basic_iterator{ cursor<false>(std::ranges::begin(base_), std::ranges::end(base_)) };
        }

        constexpr auto begin() const requires std::ranges::range<const V> {
            return basic_iterator{ cursor<true>(std::ranges::begin(base_), std::ranges::end(base_)) };
        }

        constexpr auto end() requires (!tl::simple_view<V>) {
            if constexpr (std::ranges::common_range<V>) {
                return basic_iterator{ cursor<false>{as_sentinel, std::ranges::begin(base_), std::ranges::end(base_)} };
            } else {
                return basic_sentinel<V, false>(std::ranges::end(base_));
            }
        }

        constexpr auto end() const requires std::ranges::range<const V> {
            if constexpr (std::ranges::common_range<const V>) {
                return basic_iterator{ cursor<true>(as_sentinel, std::ranges::begin(base_), std::ranges::end(base_)) };
            } else {
                return basic_sentinel<V, true>(std::ranges::end(base_));
            }
        }

        constexpr auto size() requires std::ranges::sized_range<V> {
            auto sz = std::ranges::size(base_);
            sz -= std::min<decltype(sz)>(sz, N - 1);
            return sz;
        }
        constexpr auto size() const requires std::ranges::sized_range<const V> {
            auto sz = std::ranges::size(base_);
            sz -= std::min<decltype(sz)>(sz, N - 1);
            return sz;
        }
    };

    namespace views {
        namespace detail {
            template <std::size_t N>
            class adjacent_fn {
            public:
                template <std::ranges::viewable_range V>
                    requires std::ranges::forward_range<V>
                constexpr auto operator()(V&& v) const {
                    return tl::adjacent_view<std::views::all_t<V>, N>{ std::forward<V>(v) };
                }
            };
        }  // namespace detail

        template <std::size_t N>
        inline constexpr auto adjacent = pipeable(detail::adjacent_fn<N>{});
        inline constexpr auto pairwise = pipeable(detail::adjacent_fn<2>{});
    }  // namespace views
}  // namespace tl

namespace std::ranges {
    template <class R, std::size_t N>
    inline constexpr bool enable_borrowed_range<tl::adjacent_view<R, N>> = enable_borrowed_range<R>;
}

#endif
