#ifndef TL_RANGES_ZIP_HPP
#define TL_RANGES_ZIP_HPP

#include <ranges>
#include "common.hpp"
#include "utility/tuple_utils.hpp"
#include "basic_iterator.hpp"
#include "functional/lift.hpp"

namespace tl {
   namespace detail {
      //We can't use std::ranges::equal_to for iterators and sentinels
      //because the sentinel type might not be equality comparable with itself
      struct unconstrained_equal_to {
         template <class A, class B>
         constexpr bool operator()(A&& a, B&& b) const {
            return a == b;
         }
      };
   }

   template<std::ranges::input_range... Vs>
   requires (std::ranges::view<Vs> && ...) && (sizeof...(Vs) > 0)
      class zip_view : public std::ranges::view_interface<zip_view<Vs...>> {
      template <class... Rs>
      static constexpr bool am_common = (sizeof...(Rs) == 1 && (std::ranges::common_range<Rs> && ...)) ||
         (!(std::ranges::bidirectional_range<Rs> && ...) && (std::ranges::common_range<Rs> && ...)) ||
         ((std::ranges::random_access_range<Rs> && ...) && (std::ranges::sized_range<Rs> && ...));

      private:
         std::tuple<Vs...> bases_;

      public:
         zip_view() = default;
         constexpr explicit zip_view(Vs... bases) : bases_(std::move(bases)...) {}

         template <bool Const>
         struct sentinel {
            template<class T>
            using constify = std::conditional_t<Const, const T, T>;
            tuple_or_pair<std::ranges::sentinel_t<constify<Vs>>...> end_;
         public:
            sentinel() = default;

            constexpr explicit sentinel(tuple_or_pair<std::ranges::sentinel_t<constify<Vs>>...> end) :
               end_(std::move(end)) {}

            constexpr sentinel(sentinel<!Const> i)
               requires Const && (std::convertible_to<std::ranges::sentinel_t<Vs>, std::ranges::sentinel_t<constify<Vs>>> && ...) :
               end_(std::move(i.end_)) {}
         };

         template <bool Const>
         struct cursor {
            template<class T>
            using constify = std::conditional_t<Const, const T, T>;

            using value_type = tuple_or_pair<std::ranges::range_value_t<constify<Vs>>...>;
            using difference_type = std::common_type_t<std::ranges::range_difference_t<constify<Vs>>...>;

            static constexpr bool single_pass = (detail::single_pass_iterator<std::ranges::iterator_t<constify<Vs>>> || ...);

            tuple_or_pair<std::ranges::iterator_t<constify<Vs>>...> currents_{};

            cursor() = default;
            constexpr explicit cursor(tuple_or_pair<std::ranges::iterator_t<constify<Vs>>...> currents) :
               currents_(std::move(currents)) {}

            constexpr cursor(cursor<!Const> i)
               requires Const && (std::convertible_to<std::ranges::iterator_t<Vs>, std::ranges::iterator_t<constify<Vs>>> && ...) :
               currents_(std::move(i.currents_)) {}

            constexpr decltype(auto) read() const {
               return tuple_transform([](auto& i) -> decltype(auto) { return *i; }, currents_);
            }

            constexpr void next() {
               tuple_for_each([](auto& i) { ++i; }, currents_);
            }

            constexpr void prev() requires (std::ranges::bidirectional_range<constify<Vs>> && ...) {
               tuple_for_each([](auto& i) { --i; }, currents_);
            }

            constexpr void advance(difference_type n) requires (std::ranges::random_access_range<constify<Vs>> && ...) {
               tuple_for_each([n](auto& i) { i += n; }, currents_);
            }

            constexpr bool equal(cursor const& rhs) const
               requires (std::equality_comparable<std::ranges::iterator_t<constify<Vs>>> && ...) {
               if constexpr ((std::ranges::bidirectional_range<constify<Vs>> && ...)) {
                  return currents_ == rhs.currents_;
               }
               else {
                  return tl::tuple_fold(
                     tl::tuple_transform(tl::detail::unconstrained_equal_to{}, currents_, rhs.currents_),
                     false, std::logical_or{});
               }
            }

            constexpr bool equal(sentinel<Const> const& rhs) const
               requires ((std::sentinel_for<std::ranges::sentinel_t<constify<Vs>>, std::ranges::iterator_t<constify<Vs>>>) && ...) {
               return tl::tuple_fold(
                  tl::tuple_transform(tl::detail::unconstrained_equal_to{}, currents_, rhs.end_),
                  false, std::logical_or{});
            }

            constexpr difference_type distance_to(cursor const& rhs) const
               requires ((std::sized_sentinel_for<std::ranges::iterator_t<constify<Vs>>, std::ranges::iterator_t<constify<Vs>>>) && ...) {
               auto differences = tl::tuple_transform(TL_LIFT(std::ranges::distance), currents_, rhs.currents_);
               return tl::min_tuple(differences);
            }

            constexpr difference_type distance_to(sentinel<Const> const& rhs) const
               requires ((std::sized_sentinel_for<std::ranges::sentinel_t<constify<Vs>>, std::ranges::iterator_t<constify<Vs>>>) && ...) {
               auto differences = tl::tuple_transform(TL_LIFT(std::ranges::distance), currents_, rhs.end_);
               return tl::min_tuple(differences);
            }
         };

         constexpr auto begin() requires (!(simple_view<Vs> && ...)) {
            return basic_iterator{ cursor<false>(tl::tuple_transform(std::ranges::begin, bases_)) };
         }
         constexpr auto begin() const requires (std::ranges::range<const Vs> && ...) {
            return basic_iterator{ cursor<true>(tl::tuple_transform(std::ranges::begin, bases_)) };
         }

         constexpr auto end() requires (!(simple_view<Vs> && ...) && !am_common<Vs...>) {
            return sentinel<false>(tl::tuple_transform(std::ranges::end, bases_));
         }
         constexpr auto end() requires (!(simple_view<Vs> && ...) && am_common<Vs...>) {
            if constexpr ((std::ranges::random_access_range<Vs> && ...)) {
               return begin() + size();
            }
            else {
               return basic_iterator{ cursor<false>(tl::tuple_transform(std::ranges::end, bases_)) };
            }
         }

         constexpr auto end() const requires ((std::ranges::range<const Vs> && ...) && !am_common<const Vs...>) {
            return sentinel<true>(tl::tuple_transform(std::ranges::end, bases_));
         }
         constexpr auto end() const requires ((std::ranges::range<const Vs> && ...) && am_common<const Vs...>) {
            if constexpr ((std::ranges::random_access_range<const Vs> && ...)) {
               return begin() + size();
            }
            else {
               return basic_iterator{ cursor<true>(tl::tuple_transform(std::ranges::end, bases_)) };
            }
         }

         constexpr auto size() requires (std::ranges::sized_range<Vs> && ...) {
            return tl::min_tuple(tl::tuple_transform(std::ranges::size, bases_));
         }
         constexpr auto size() const requires (std::ranges::sized_range<const Vs> && ...) {
            return tl::min_tuple(tl::tuple_transform(std::ranges::size, bases_));
         }

   };

   template <class... Rs>
   zip_view(Rs&&...)->zip_view<std::views::all_t<Rs>...>;

   namespace views {
      namespace detail {
         class zip_fn {
         public:
            constexpr std::ranges::empty_view<std::tuple<>> operator()() const noexcept {
               return {};
            }
            template <std::ranges::viewable_range... V>
            requires ((std::ranges::input_range<V> && ...) && (sizeof...(V) != 0))
               constexpr auto operator()(V&&... vs) const {
               return tl::zip_view{ std::views::all(std::forward<V>(vs))... };
            }
         };
      }  // namespace detail

      inline constexpr detail::zip_fn zip;
   }  // namespace views
}

namespace std::ranges {
   template<class... Vs>
   inline constexpr bool enable_borrowed_range<tl::zip_view<Vs...>>
      = (enable_borrowed_range<Vs> && ...);
}

#endif