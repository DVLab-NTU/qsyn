#ifndef TL_RANGES_REPEAT_HPP
#define TL_RANGES_REPEAT_HPP

#include <iterator>
#include <ranges>
#include <type_traits>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"
#include "utility/semiregular_box.hpp"

namespace tl {
   template <std::movable T>
   class repeat_view
      : public std::ranges::view_interface<repeat_view<T>> {
   private:
      tl::semiregular_box<T> value_;

      class cursor {
         T const* value_;
         std::ptrdiff_t pos_ = 0;

      public:
         static constexpr bool single_pass = false;

         cursor() = default;
         constexpr explicit cursor(T const* value)
            : value_{ value }, pos_{ 0 } {}

         constexpr decltype(auto) read() const {
            return *value_;
         }

         constexpr void next() {
            ++pos_;
         }

         constexpr void prev() {
            --pos_;
         }
         constexpr void advance(std::ptrdiff_t x){
            pos_ += x;
         }

         constexpr bool equal(const cursor& rhs) const {
            return pos_ == rhs.pos_;
         }

         constexpr auto distance_to(const cursor& rhs) const {
            return rhs.pos_ - pos_;
         }
      };

   public:
      repeat_view() = default;
      repeat_view(T t) : value_(std::move(t)) {}

      constexpr auto begin() const {
         return basic_iterator{ cursor(std::addressof(value_.value())) };
      }

      constexpr auto end() const {
         return std::unreachable_sentinel;
      }
   };

   template <class T>
   repeat_view(T&&)->repeat_view<T>;

   namespace views {
      namespace detail {
         class repeat_fn {
         public:
            template <std::copyable T>
            constexpr auto operator()(T&& v) const {
               return tl::repeat_view{ std::forward<T>(v) };
            }
         };
      }  // namespace detail

      inline constexpr auto repeat = detail::repeat_fn{};
   }  // namespace views
}  // namespace tl

namespace std::ranges {
   template <class R>
   inline constexpr bool enable_borrowed_range<tl::repeat_view<R>> = enable_borrowed_range<R>;
}

#endif