#ifndef TL_RANGES_CYCLE_HPP
#define TL_RANGES_CYCLE_HPP

#include <iterator>
#include <ranges>
#include <type_traits>
#include "functional/pipeable.hpp"
#include "basic_iterator.hpp"

namespace tl {
   template <std::ranges::forward_range V>
   requires (std::ranges::view<V>) class cycle_view
      : public std::ranges::view_interface<cycle_view<V>> {
      //We need to be able to get to end from begin in O(1).
      template <class T>
      static constexpr bool am_bidirectional = (std::ranges::bidirectional_range<T> &&
         (std::ranges::common_range<T> || (std::ranges::sized_range<T> && std::ranges::random_access_range<T>)));

      //Need to be able to calculate offset modulo the size of the range.
      template <class T>
      static constexpr bool am_random_access = (std::ranges::random_access_range<T> && std::ranges::sized_range<T>);

      V base_;

      template <bool Const>
      class cursor {
         using Base = std::conditional_t<Const, const V, V>;

         std::ranges::iterator_t<Base> current_{};
         Base* base_ = nullptr;
 
      public:
         using difference_type = std::ranges::range_difference_t<Base>;

         cursor() = default;
         constexpr explicit cursor(std::ranges::iterator_t<Base> current, Base* base)
            : current_{ std::move(current) }, base_{ base }  {}

         //const-converting constructor
         constexpr cursor(cursor<!Const> i) requires Const&& std::convertible_to<
            std::ranges::iterator_t<V>,
            std::ranges::iterator_t<Base>>
            : current_{ std::move(i.current_) } {}

         constexpr std::ranges::iterator_t<Base> base()
            const& requires std::copyable<std::ranges::iterator_t<Base>> {
            return current_;
         }
         constexpr std::ranges::iterator_t<Base> base()&& {
            return std::move(current_);
         }

         constexpr decltype(auto) read() const {
            return *current_;
         }

         constexpr void next() {
            ++current_;
            if (current_ == std::ranges::end(*base_)) {
               current_ = std::ranges::begin(*base_);
            }
         }

         constexpr void prev() requires am_bidirectional<Base> {
            if (current_ == std::ranges::begin(*base_)) {
               current_ = std::ranges::end(*base_);
            }
            --current_;
         }
     
         constexpr void advance(difference_type x) requires am_random_access<Base> {
            auto begin = std::ranges::begin(*base_);
            auto end = std::ranges::end(*base_);
            auto size = std::ranges::size(*base_);
            auto distance_from_begin = current_ - begin;
            auto offset = (distance_from_begin + x) % size;
            current_ = begin + static_cast<difference_type>(offset < 0 ? offset + size : offset);
         }

         constexpr bool equal(const cursor& rhs) const requires std::
            equality_comparable<std::ranges::iterator_t<Base>> {
            return current_ == rhs.current_;
         }

         constexpr auto distance_to(const cursor& rhs) const
            requires std::sized_sentinel_for<std::ranges::iterator_t<Base>, std::ranges::iterator_t<Base>> {
            return rhs.current_ - current_;
         }

         friend class cursor<!Const>;
      };

   public:
      cycle_view() = default;
      cycle_view(V base) : base_(std::move(base)) {}

      constexpr auto begin() {
         return basic_iterator{ cursor<false>(std::ranges::begin(base_), std::addressof(base_)) };
      }
      constexpr auto begin() const  {
         return basic_iterator{ cursor<true>(std::ranges::begin(base_), std::addressof(base_)) };
      }

      constexpr auto end() const { return std::unreachable_sentinel; }

      constexpr V base() const& requires std::copy_constructible<V> {
         return base_;
      }
      constexpr V base()&& { return std::move(base_); }
   };

   template <class R>
   cycle_view(R&&)->cycle_view<std::views::all_t<R>>;

   namespace views {
      namespace detail {
         struct cycle_fn {
            template <std::ranges::viewable_range V>
            constexpr auto operator()(V&& v) const 
            requires (std::ranges::forward_range<V> && 
               (std::ranges::common_range<V> || !std::ranges::bidirectional_range<V>)) {
               return tl::cycle_view{ std::forward<V>(v) };
            }
         };
      }  // namespace detail

      inline constexpr auto cycle = pipeable(detail::cycle_fn{});
   }  // namespace views
}  // namespace tl

#endif