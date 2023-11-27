#ifndef TL_RANGES_ENUMERATE_HPP
#define TL_RANGES_ENUMERATE_HPP

#include <iterator>
#include <ranges>
#include <type_traits>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"

namespace tl {
   template <std::ranges::input_range V>
   requires std::ranges::view<V> class enumerate_view
      : public std::ranges::view_interface<enumerate_view<V>> {
   public:
      template <class T>
      static constexpr bool am_common = std::ranges::common_range<T> && std::ranges::sized_range<T>;

   private:
      V base_;

      template <bool Const>
      class cursor {
         using Base = std::conditional_t<Const, const V, V>;
         using count_type = decltype([] {
            if constexpr (std::ranges::sized_range<Base>)
               return std::ranges::range_size_t<Base>();
            else {
               return std::make_unsigned_t<std::ranges::range_difference_t<Base>>();
            }
            }());

         std::ranges::iterator_t<Base> current_{};
         count_type pos_ = 0;

      public:
         static constexpr bool single_pass = detail::single_pass_iterator<std::ranges::iterator_t<Base>>;
         using reference =
            std::pair<count_type, std::ranges::range_reference_t<Base>>;

         using difference_type = std::ranges::range_difference_t<Base>;

         cursor() = default;
         constexpr explicit cursor(std::ranges::iterator_t<Base> current,
            difference_type pos)
            : current_{ std::move(current) }, pos_{ static_cast<count_type>(pos) } {}

         constexpr cursor(cursor<!Const> i) requires Const&& std::convertible_to<
            std::ranges::iterator_t<V>,
            std::ranges::iterator_t<Base>>
            : current_{ std::move(i.current_) }, pos_{ i.pos_ } {}

         constexpr decltype(auto) read() const {
            return reference{ pos_, *current_ };
         }

         constexpr void next() {
            ++current_;
            ++pos_;
         }

         constexpr void prev() requires std::ranges::bidirectional_range<Base> {
            --pos_;
            --current_;
         }
         constexpr void advance(difference_type x) requires std::ranges::random_access_range<Base> {
            pos_ += x;
            current_ += x;
         }

         constexpr bool equal(const cursor& rhs) const requires std::equality_comparable<std::ranges::iterator_t<Base>> {
            return current_ == rhs.current_;
         }

         constexpr bool equal(const basic_sentinel<V, Const>& rhs) const {
            return current_ == rhs.end();
         }

         constexpr auto distance_to(const cursor& rhs) const
            requires std::sized_sentinel_for<std::ranges::iterator_t<Base>, std::ranges::iterator_t<Base>> {
            return rhs.current_ - current_;
         }

         constexpr auto distance_to(const basic_sentinel<V, Const>& rhs) const
            requires std::sized_sentinel_for<std::ranges::sentinel_t<Base>, std::ranges::iterator_t<Base>> {
            return rhs.end_ - current_;
         }

         friend class cursor<!Const>;
      };

   public:
      enumerate_view() = default;
      enumerate_view(V base) : base_(std::move(base)) {}

      constexpr auto begin() requires(!simple_view<V>) {
         return basic_iterator{ cursor<false>(std::ranges::begin(base_), 0) };
      }
      constexpr auto begin() const requires std::ranges::range<const V> {
         return basic_iterator{ cursor<true>(std::ranges::begin(base_), 0) };
      }

      constexpr auto end() requires(!simple_view<V> && !am_common<V>) {
         return basic_sentinel<V, false>{std::ranges::end(base_)};
      }

      constexpr auto end() requires (!simple_view<V>&& am_common<V>) {
         return basic_iterator{ cursor<false>{
            std::ranges::end(base_),
               static_cast<std::ranges::range_difference_t<V>>(size())} };
      }

      constexpr auto end() const requires (std::ranges::range<const V> && !am_common<const V>) {
         return basic_sentinel<V, true>{std::ranges::end(base_)};
      }

      constexpr auto end() const requires am_common<const V> {
         return basic_iterator{ cursor<true>{
            std::ranges::end(base_),
               static_cast<std::ranges::range_difference_t<V>>(size())} };
      }

      constexpr auto size() requires std::ranges::sized_range<V> {
         return std::ranges::size(base_);
      }
      constexpr auto size() const requires std::ranges::sized_range<const V> {
         return std::ranges::size(base_);
      }

      constexpr V base() const& requires std::copy_constructible<V> {
         return base_;
      }
      constexpr V base()&& { return std::move(base_); }
   };

   template <class R>
   enumerate_view(R&&)->enumerate_view<std::views::all_t<R>>;

   namespace views {
      namespace detail {
         class enumerate_fn {
         public:
            template <std::ranges::viewable_range V>
            constexpr auto operator()(V&& v) const
               requires std::ranges::input_range<V> {
               return tl::enumerate_view{ std::forward<V>(v) };
            }
         };
      }  // namespace detail

      inline constexpr auto enumerate = pipeable(detail::enumerate_fn{});
   }  // namespace views
}  // namespace tl

namespace std::ranges {
   template <class R>
   inline constexpr bool enable_borrowed_range<tl::enumerate_view<R>> = enable_borrowed_range<R>;
}

#endif