#ifndef TL_RANGES_GENERATE_HPP
#define TL_RANGES_GENERATE_HPP

#include <iterator>
#include <ranges>
#include <type_traits>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"
#include "utility/semiregular_box.hpp"
#include "utility/non_propagating_cache.hpp"

namespace tl {
   template <std::invocable<> F>
   requires std::is_object_v<F> class generate_view
      : public std::ranges::view_interface<generate_view<F>> {
   private:
      [[no_unique_address]] tl::semiregular_storage_for<F> func_;
      tl::non_propagating_cache<std::invoke_result_t<F>> cache_;

      class cursor {
         generate_view* parent_;
         std::ptrdiff_t pos_ = 0;

      public:
         static constexpr bool single_pass = true;

         cursor() = default;
         constexpr explicit cursor(generate_view* parent)
            : parent_{ parent }, pos_{ 0 } {}

         constexpr decltype(auto) read() const {
            if (!parent_->cache_) {
               parent_->cache_.emplace(std::invoke(parent_->func_));
            }
            return parent_->cache_.value();
         }

         constexpr void next() {
            ++pos_;
            if (parent_->cache_) {
               parent_->cache_.reset();
            }
            else {
               (void)std::invoke(parent_->func_);
            }
         }

         constexpr bool equal(const cursor& rhs) const {
            return pos_ == rhs.pos_;
         }

         constexpr auto distance_to(const cursor& rhs) const {
            return rhs.pos_ - pos_;
         }
      };

   public:
      generate_view() = default;
      generate_view(F f) : func_(std::move(f)) {}

      constexpr auto begin() {
         return basic_iterator{ cursor(this) };
      }

      constexpr auto end() const {
         return std::unreachable_sentinel;
      }
   };

   template <class T>
   generate_view(T&&)->generate_view<T>;

   namespace views {
      namespace detail {
         class generate_fn {
         public:
            template <std::invocable<> F>
            constexpr auto operator()(F&& f) const {
               return tl::generate_view{ std::forward<F>(f) };
            }
         };
      }  // namespace detail

      inline constexpr auto generate = detail::generate_fn{};
   }  // namespace views
}  // namespace tl

#endif