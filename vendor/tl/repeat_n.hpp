#ifndef TL_RANGES_REPEAT_N_HPP
#define TL_RANGES_REPEAT_N_HPP

#include <iterator>
#include <ranges>
#include <type_traits>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"
#include "utility/semiregular_box.hpp"

namespace tl {
   template <std::movable T>
   class repeat_n_view
      : public std::ranges::view_interface<repeat_n_view<T>> {
   private:
      tl::semiregular_box<T> value_;
      std::size_t n_;

      class cursor {
         T const* value_;
         std::size_t pos_ = 0;

      public:
         static constexpr bool single_pass = false;

         cursor() = default;
         constexpr explicit cursor(T const* value, std::size_t pos)
            : value_{ value }, pos_{ pos } {}

         constexpr decltype(auto) read() const {
            return *value_;
         }

         constexpr void next() {
            --pos_;
         }

         constexpr void prev() {
            ++pos_;
         }
         constexpr void advance(std::ptrdiff_t x) {
            pos_ -= x;
         }

         constexpr bool equal(const cursor& rhs) const {
            return pos_ == rhs.pos_;
         }

         constexpr bool equal(std::default_sentinel_t) const {
            return pos_ == 0;
         }

         constexpr auto distance_to(const cursor& rhs) const {
            return rhs.pos_ - pos_;
         }

         constexpr auto distance_to(std::default_sentinel_t rhs) const {
            return pos_;
         }
      };

   public:
      repeat_n_view() = default;
      repeat_n_view(T t, std::size_t n) : value_(std::move(t)), n_(n) {}

      constexpr auto begin() const {
         return basic_iterator{ cursor(std::addressof(value_.value()), n_) };
      }

      constexpr auto end() const {
         return std::default_sentinel;
      }
   };

   template <class T>
   repeat_n_view(T&&, std::size_t)->repeat_n_view<T>;

   namespace views {
      namespace detail {
         class repeat_n_fn {
         public:
            template <std::copyable T>
            constexpr auto operator()(T&& v, std::size_t n) const {
               return tl::repeat_n_view{ std::forward<T>(v), n };
            }
         };
      }  // namespace detail

      inline constexpr auto repeat_n = detail::repeat_n_fn{};
   }  // namespace views
}  // namespace tl

namespace std::ranges {
   template <class R>
   inline constexpr bool enable_borrowed_range<tl::repeat_n_view<R>> = enable_borrowed_range<R>;
}

#endif