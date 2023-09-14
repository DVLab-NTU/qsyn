#ifndef TL_RANGES_CHUNK_BY
#define TL_RANGES_CHUNK_BY

#include <ranges>
#include <iterator>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "utility/semiregular_box.hpp"
#include "utility/non_propagating_cache.hpp"
#include "functional/bind.hpp"
#include "functional/pipeable.hpp"

namespace tl {
   template <std::ranges::forward_range V, std::predicate<std::ranges::range_reference_t<V>, std::ranges::range_reference_t<V>> F>
   requires std::ranges::view<V>
      class chunk_by_view
      : public std::ranges::view_interface<chunk_by_view<V, F>> {
      private:
         V base_;
         //Might need to wrap F in a semiregular_box to ensure the view is moveable and default-initializable
         [[no_unique_address]] semiregular_storage_for<F> func_;
         //Need to cache the end of the first group so that begin is amortized O(1)
         non_propagating_cache<std::ranges::iterator_t<V>> end_of_first_group_;

         //The first time we call begin or when a cursor is advanced we calculate the end of the current range
         //by walking over the range until we find an adjacent pair that returns false for the predicate.
         constexpr std::ranges::iterator_t<V> find_end_of_current_range(std::ranges::iterator_t<V> it) {
            auto first_failed = std::adjacent_find(it, std::ranges::end(base_), std::not_fn(func_));
            return std::ranges::next(first_failed, 1, std::ranges::end(base_));
         }

         constexpr auto get_end_of_first_group() {
            if (!end_of_first_group_) {
               end_of_first_group_ = find_end_of_current_range(std::ranges::begin(base_));
            }
            return *end_of_first_group_;
         }

         struct sentinel {
            std::ranges::sentinel_t<V> end_;
            sentinel() = default;
            constexpr sentinel(std::ranges::sentinel_t<V> end) : end_(std::move(end)) {}
         };

         struct cursor {
            std::ranges::iterator_t<V> current_;
            std::ranges::iterator_t<V> end_of_current_range_;
            chunk_by_view* parent_;

            cursor() = default;
            constexpr cursor(std::ranges::iterator_t<V> begin, std::ranges::iterator_t<V> end_of_first_group, chunk_by_view* parent)
               : current_(std::move(begin)), end_of_current_range_(std::move(end_of_first_group)), parent_(parent) {
            }

            constexpr auto read() const {
               return std::ranges::subrange{ current_, end_of_current_range_ };
            }

            constexpr void next() {
               current_ = std::exchange(end_of_current_range_, parent_->find_end_of_current_range(end_of_current_range_));
            }

            constexpr bool equal(cursor const& rhs) const {
               return current_ == rhs.current_;
            }
            constexpr bool equal(sentinel const& rhs) const {
               return current_ == rhs.end_;
            }
         };

      public:
         chunk_by_view() = default;
         chunk_by_view(V v, F f) : base_(std::move(v)), func_(std::move(f)) {}

         constexpr auto begin() {
            return basic_iterator{ cursor{ std::ranges::begin(base_), get_end_of_first_group(), this } };
         }

         constexpr auto end() {
            return sentinel{std::ranges::end(base_)};
         }

         auto& base() {
            return base_;
         }
   };

   template <class R, class F>
   chunk_by_view(R&&, F f)->chunk_by_view<std::views::all_t<R>, F>;

   namespace views {
      namespace detail {
         struct chunk_by_fn_base {
            template <std::ranges::viewable_range R, std::predicate<std::ranges::range_reference_t<R>, std::ranges::range_reference_t<R>> F>
            constexpr auto operator()(R&& r, F f) const
               requires std::ranges::forward_range<R> {
               return chunk_by_view(std::forward<R>(r), std::move(f));
            }
         };

         struct chunk_by_fn : chunk_by_fn_base {
            using chunk_by_fn_base::operator();

            template <class F>
            constexpr auto operator()(F f) const {
               return pipeable(bind_back(chunk_by_fn_base{}, std::move(f)));
            }
         };
      }

      constexpr inline detail::chunk_by_fn chunk_by;
   }
}

#endif