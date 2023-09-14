#ifndef TL_RANGES_CHUNK_BY_KEY
#define TL_RANGES_CHUNK_BY_KEY

#include <ranges>
#include <iterator>
#include <utility>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "utility/semiregular_box.hpp"
#include "utility/non_propagating_cache.hpp"
#include "functional/bind.hpp"
#include "functional/pipeable.hpp"

namespace tl {
   template <std::ranges::forward_range V, std::invocable<std::ranges::range_reference_t<V>> F>
   requires std::ranges::view<V>
   class chunk_by_key_view 
      : public std::ranges::view_interface<chunk_by_key_view<V,F>> {
   private:
      using key_type = std::invoke_result_t<F, std::ranges::range_reference_t<V>>;

      V base_;
      //Might need to wrap F in a semiregular_box to ensure the view is moveable and default-initializable
      [[no_unique_address]] semiregular_storage_for<F> func_;
      //Need to cache the end of the first group so that begin is amortized O(1). Also cache the first key.
      non_propagating_cache<std::pair<key_type, std::ranges::iterator_t<V>>> first_group_info_;

      constexpr auto get_first_group_info() {
         if (!first_group_info_) {
            auto it = std::ranges::begin(base_);
            if (it != std::ranges::end(base_)) {
               auto first_key = std::invoke(func_, *it);
               auto end_of_first_range = std::ranges::find_if(std::ranges::next(it), std::end(base_), [&](auto&& v) { return std::invoke(func_, v) != first_key; });

               first_group_info_ = std::pair(std::move(first_key), std::move(end_of_first_range));
            }
         }
         return *first_group_info_;
      }

      struct sentinel {
         std::ranges::sentinel_t<V> end_;
         sentinel() = default;
         constexpr sentinel(std::ranges::sentinel_t<V> end) : end_(std::move(end)) {}
      };

      struct cursor {
         std::ranges::iterator_t<V> current_;
         std::ranges::iterator_t<V> end_of_current_range_;
         std::optional<key_type> current_key_;
         chunk_by_key_view* parent_;

         cursor() = default;
         constexpr cursor(std::ranges::iterator_t<V> begin, std::ranges::iterator_t<V> end_of_first_group, key_type first_key, chunk_by_key_view* parent)
            : current_(std::move(begin)), end_of_current_range_(std::move(end_of_first_group)), current_key_(std::move(first_key)), parent_(parent) {
         }

         constexpr auto read() const {
            return std::pair(*current_key_, std::ranges::subrange{ current_, end_of_current_range_ });
         }

         constexpr void next() {
            current_ = end_of_current_range_;
            if (current_ == std::ranges::end(parent_->base_)) return;

            current_key_ = std::invoke(parent_->func_, *current_);
            end_of_current_range_ = std::ranges::find_if(std::ranges::next(current_), std::ranges::end(parent_->base_), 
               [this](auto&& v) { return std::invoke(parent_->func_, v) != *current_key_; });
         }
   
         constexpr bool equal(cursor const& rhs) const {
            return current_ == rhs.current_;
         }
         constexpr bool equal(sentinel const& rhs) const {
            return current_ == rhs.end_;
         }
      };

   public:
      chunk_by_key_view() = default;
      chunk_by_key_view(V v, F f) : base_(std::move(v)), func_(std::move(f)) {}

      constexpr auto begin() {
         auto [first_key, end_of_first_group] = get_first_group_info();
         return basic_iterator{ cursor{ std::ranges::begin(base_), std::move(end_of_first_group), std::move(first_key), this } };
      }

      constexpr auto end() {
         return sentinel{std::ranges::end(base_)};
      }

      auto& base() {
         return base_;
      }
   };

   template <class R, class F>
   chunk_by_key_view(R&&, F f)->chunk_by_key_view<std::views::all_t<R>, F>;

   namespace views {
      namespace detail {
         struct chunk_by_key_fn_base {
            template <std::ranges::viewable_range R, std::invocable<std::ranges::range_reference_t<R>> F>
            constexpr auto operator()(R&& r, F f) const 
               requires std::ranges::forward_range<R> {
               return chunk_by_key_view(std::forward<R>(r), std::move(f));
            }
         };

         struct chunk_by_key_fn : chunk_by_key_fn_base {
            using chunk_by_key_fn_base::operator(); 

            template <class F>
            constexpr auto operator()(F f) const {
               return pipeable(bind_back(chunk_by_key_fn_base{}, std::move(f)));
            }
         };
      }

      constexpr inline detail::chunk_by_key_fn chunk_by_key;
   }
}

#endif