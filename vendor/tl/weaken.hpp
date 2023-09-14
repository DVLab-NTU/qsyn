#ifndef TL_RANGES_WEAKEN_HPP
#define TL_RANGES_WEAKEN_HPP

#include <ranges>
#include "common.hpp"
#include "functional/pipeable.hpp"
#include "basic_iterator.hpp"

namespace tl {
   enum weakening {
      non_common,
      non_sized,
      non_const_iterable,
      non_borrowable,
      random_access,
      bidirectional,
      forward,
      input
   };
   template <std::ranges::input_range V, weakening... Weakenings>
   requires std::ranges::view<V> class weaken_view
      : public std::ranges::view_interface<weaken_view<V>> {
      V base_;

      template <weakening W>
      static constexpr bool weakening_enabled = ((W == Weakenings) || ...);

      template <class T>
      static constexpr bool am_common = std::ranges::common_range<T> && !weakening_enabled<weakening::non_common>;
      template <class T>
      static constexpr bool am_sized = std::ranges::sized_range<T>;
      static constexpr bool am_const_iterable = std::ranges::range<const V> && !weakening_enabled<weakening::non_const_iterable>;
      template <class T>
      static constexpr bool am_random_access = std::ranges::random_access_range<T> 
         && !weakening_enabled<weakening::bidirectional>
         && !weakening_enabled<weakening::forward>
         && !weakening_enabled<weakening::input>;
      template <class T>
      static constexpr bool am_bidirectional = std::ranges::bidirectional_range<T> 
         && !weakening_enabled<weakening::forward>
         && !weakening_enabled<weakening::input>;

   public:
      weaken_view() = default;
      weaken_view(V base) : base_(std::move(base)) {}

      template <bool Const>
      class sentinel {
         using Base = std::conditional_t<Const, const V, V>;
         std::ranges::sentinel_t<Base> end_{};

      public:
         sentinel() = default;
         constexpr explicit sentinel(std::ranges::sentinel_t<Base> end)
            : end_{ std::move(end) } {}

         constexpr sentinel(sentinel<!Const> other) requires Const&& std::
            convertible_to<std::ranges::sentinel_t<V>,
            std::ranges::sentinel_t<Base>>
            : end_{ std::move(other.end_) } {}

         constexpr auto end() const {
            return end_;
         }

         friend class sentinel<!Const>;
      };

      template <bool Const>
      class cursor {
         using Base = std::conditional_t<Const, const V, V>;
         std::ranges::iterator_t<Base> current_{};

      public:
         static constexpr bool single_pass = 
            detail::single_pass_iterator<std::ranges::iterator_t<Base>> || weakening_enabled<weakening::input>;

         cursor() = default;
         constexpr explicit cursor(std::ranges::iterator_t<Base> current)
            : current_{ std::move(current) } {}

         constexpr cursor(cursor<!Const> i) requires Const&& std::convertible_to<
            std::ranges::iterator_t<V>,
            std::ranges::iterator_t<Base>>
            : current_{ std::move(i.current_) } {}

         constexpr decltype(auto) read() const {
            return *current_;
         }

         constexpr void next() {
            ++current_;
         }

         constexpr void prev() requires am_bidirectional<Base>{
            --current_;
         }
         constexpr void advance(std::ranges::range_difference_t<Base> x) requires am_random_access<Base> {
            current_ += x;
         }

         constexpr bool equal(const cursor& rhs) const requires std::equality_comparable<std::ranges::iterator_t<Base>> {
            return current_ == rhs.current_;
         }

         constexpr bool equal(const sentinel<Const>& rhs) const {
            return current_ == rhs.end();
         }

         constexpr auto distance_to(const cursor& rhs) const
            requires (std::sized_sentinel_for<std::ranges::iterator_t<Base>, std::ranges::iterator_t<Base>>
            && !(weakening_enabled<weakening::non_sized> && am_common<Base>)) {
            return rhs.current_ - current_;
         }

         constexpr auto distance_to(const sentinel<Const>& rhs) const
            requires (std::sized_sentinel_for<std::ranges::sentinel_t<Base>, std::ranges::iterator_t<Base>>
         && !weakening_enabled<weakening::non_sized>) {
            return rhs.end() - current_;
         }

         friend class cursor<!Const>;
      };

      constexpr auto begin() requires(!simple_view<V> || !am_const_iterable) {
         return tl::basic_iterator{ cursor<false>(std::ranges::begin(base_)) };
      }
      constexpr auto begin() const requires am_const_iterable {
         return tl::basic_iterator{ cursor<true>(std::ranges::begin(base_)) };
      }

      constexpr auto end() requires((!simple_view<V> || !am_const_iterable) && !am_common<V>) {
         return sentinel<false>{std::ranges::end(base_)};
      }

      constexpr auto end() requires ((!simple_view<V> || !am_const_iterable) && am_common<V>) {
         return basic_iterator{ cursor<false>{std::ranges::end(base_)} };
      }

      constexpr auto end() const requires (am_const_iterable && !am_common<const V>) {
         return sentinel<true>{std::ranges::end(base_)};
      }

      constexpr auto end() const requires am_common<const V> {
         return basic_iterator{ cursor<true>{std::ranges::end(base_)} };
      }

      constexpr auto size() requires am_sized<V> {
         return std::ranges::size(base_);
      }
      constexpr auto size() const requires am_sized<const V> {
         return std::ranges::size(base_);
      }

      constexpr V base() const& requires std::copy_constructible<V> {
         return base_;
      }
      constexpr V base()&& { return std::move(base_); }
   };

   namespace views {
      namespace detail {
         template <tl::weakening... Weakenings>
         class weaken_fn {
         public:
            template <std::ranges::viewable_range V>
            constexpr auto operator()(V&& v) const
               requires std::ranges::input_range<V> {
               return tl::weaken_view<std::views::all_t<V>, Weakenings...>{ std::forward<V>(v) };
            }
         };
      }  // namespace detail

      template<weakening... Weakenings>
      inline constexpr auto weaken = pipeable(detail::weaken_fn<Weakenings...>{});
   }  // namespace views
}  // namespace tl

namespace std::ranges {
   template <class R, tl::weakening... Weakenings>
   inline constexpr bool enable_borrowed_range<tl::weaken_view<R, Weakenings...>> = 
      enable_borrowed_range<R> && !((tl::weakening::non_borrowable == Weakenings) || ...);

   template <class R, tl::weakening... Weakenings>
   inline constexpr bool disable_sized_range<tl::weaken_view<R, Weakenings...>> = 
      ((tl::weakening::non_sized == Weakenings) || ...);
}

#endif