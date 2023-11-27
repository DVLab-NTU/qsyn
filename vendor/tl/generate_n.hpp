#ifndef TL_RANGES_GENERATE_N_HPP
#define TL_RANGES_GENERATE_N_HPP

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
   requires std::is_object_v<F> class generate_n_view
      : public std::ranges::view_interface<generate_n_view<F>> {
   private:
      [[no_unique_address]] tl::semiregular_storage_for<F> func_;
      tl::non_propagating_cache<std::invoke_result_t<F>> cache_;
      std::size_t n_;

      class cursor {
         generate_n_view* parent_;
         std::size_t pos_ = 0;

      public:
         static constexpr bool single_pass = true;

         cursor() = default;
         constexpr explicit cursor(generate_n_view* parent, std::size_t n)
            : parent_{ parent }, pos_{ n } {}

         constexpr decltype(auto) read() const {
            if (!parent_->cache_) {
               parent_->cache_.emplace(std::invoke(parent_->func_));
            }
            return parent_->cache_.value();
         }

         constexpr void next() {
            --pos_;
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

         constexpr bool equal(std::default_sentinel_t) const {
            return pos_ == 0;
         }

         constexpr auto distance_to(const cursor& rhs) const {
            return static_cast<std::ptrdiff_t>(rhs.pos_) - static_cast<std::ptrdiff_t>(pos_);
         }

         constexpr auto distance_to(std::default_sentinel_t) const {
            return static_cast<std::ptrdiff_t>(pos_);
         }
      };

   public:
      generate_n_view() = default;
      generate_n_view(F f, std::size_t n) : func_(std::move(f)), n_(n) {}

      constexpr auto begin() {
         return basic_iterator{ cursor(this, n_) };
      }

      constexpr auto end() const {
         return std::default_sentinel;
      }
   };

   template <class T>
   generate_n_view(T&&, std::size_t)->generate_n_view<T>;

   namespace views {
      namespace detail {
         class generate_n_fn {
         public:
            template <std::invocable<> F>
            constexpr auto operator()(F&& f, std::size_t n) const {
               return tl::generate_n_view{ std::forward<F>(f), n };
            }
         };
      }  // namespace detail

      inline constexpr auto generate_n = detail::generate_n_fn{};
   }  // namespace views
}  // namespace tl

#endif