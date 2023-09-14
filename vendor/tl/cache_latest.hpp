#ifndef TL_RANGES_CACHE_LATEST_HPP
#define TL_RANGES_CACHE_LATEST_HPP

#include <ranges>
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"
#include "utility/non_propagating_cache.hpp"

namespace tl {
   template <std::ranges::input_range V>
   requires (std::ranges::view<V>&& std::constructible_from<std::ranges::range_value_t<V>, std::ranges::range_reference_t<V>>)
      struct cache_latest_view
      : public std::ranges::view_interface<cache_latest_view<V>> {
      V base_;
      non_propagating_cache<std::ranges::range_value_t<V>> cache_ = std::nullopt;
      bool dirty_ = true;

      template <class T>
      void update(T&& value) {
         cache_ = std::forward<T>(value);
         dirty_ = false;
      }

      struct cursor {
         std::ranges::iterator_t<V> current_;
         cache_latest_view* parent_;

         constexpr static bool single_pass = true;

         cursor() = default;
         constexpr cursor(std::ranges::iterator_t<V> current, cache_latest_view* parent)
            : current_(std::move(current)), parent_(parent) {}

         constexpr std::ranges::range_value_t<V>& read() const {
            if (parent_->dirty_) {
               parent_->update(*current_);
            }
            return *parent_->cache_;
         }

         constexpr void next() {
            ++current_;
            parent_->dirty_ = true;
         }

         constexpr bool equal(cursor const& rhs) const {
            return current_ == rhs.current_;
         }

         constexpr bool equal(std::ranges::sentinel_t<V> const& rhs) const {
            return current_ == rhs;
         }

         constexpr auto distance_to(cursor const& rhs) const requires
            std::sized_sentinel_for<std::ranges::iterator_t<V>, std::ranges::iterator_t<V>> {
            return rhs.current_ - current_;
         }

         constexpr auto distance_to(std::ranges::sentinel_t<V> const& rhs) const requires
            std::sized_sentinel_for<std::ranges::iterator_t<V>, std::ranges::sentinel_t<V>> {
            return rhs - current_;
         }
      };

      constexpr auto begin() {
         return basic_iterator{ cursor{std::ranges::begin(base_), this} };
      }

      constexpr auto end() requires std::ranges::common_range<V> {
         return basic_iterator{ cursor{std::ranges::end(base_), this} };
      }

      constexpr auto end() {
         return std::ranges::end(base_);
      }

      cache_latest_view() = default;
      cache_latest_view(V base) : base_(std::move(base)) {}

      constexpr auto size() requires std::ranges::sized_range<V> {
         return std::ranges::size(base_);
      }
   };


   template <class R>
   cache_latest_view(R&&)->cache_latest_view<std::views::all_t<R>>;

   namespace views {
      namespace detail {
         class cache_latest_fn {
         public:
            template <std::ranges::viewable_range V>
            constexpr auto operator()(V&& v) const
               requires (std::ranges::input_range<V>&& std::constructible_from<std::ranges::range_value_t<V>, std::ranges::range_reference_t<V>>) {
               return tl::cache_latest_view{ std::forward<V>(v) };
            }
         };
      }  // namespace detail

      inline constexpr auto cache_latest = pipeable(detail::cache_latest_fn{});
   }
}
#endif