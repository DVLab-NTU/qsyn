#ifndef TL_RANGES_TRANSFORM_MAYBE
#define TL_RANGES_TRANSFORM_MAYBE

#include <ranges>
#include <concepts>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "utility/semiregular_box.hpp"
#include "functional/bind.hpp"
#include "functional/pipeable.hpp"
#include "utility/non_propagating_cache.hpp"

namespace tl {
   template <std::ranges::input_range V, std::invocable<std::ranges::range_reference_t<V>> F>
   requires std::ranges::view<V>
   class transform_maybe_view : public std::ranges::view_interface<transform_maybe_view<V,F>> {
   private:
      V base_;
      //Might need to wrap F in a semiregular_box to ensure the view is moveable and default-initializable
      [[no_unique_address]] semiregular_storage_for<F> func_;
      //Need to cache begin so that begin(transform_maybe_view) is amortized O(1)
      non_propagating_cache<std::ranges::iterator_t<V>> begin_;

      //Work out which is the first element which returns an engaged optional and cache it
      auto get_begin() {
         if (begin_) return *begin_;

         auto it = std::ranges::begin(base_);
         while (it != std::ranges::end(base_) && !std::invoke(func_, *it)) {
            ++it;
         };
         begin_.emplace(std::move(it));
         return *begin_;
      }

      struct sentinel {
         std::ranges::sentinel_t<V> end_;
         sentinel() = default;
         constexpr sentinel(std::ranges::sentinel_t<V> end) : end_(std::move(end)) {}
      };

      struct cursor {
         static constexpr inline bool single_pass = detail::single_pass_iterator<std::ranges::iterator_t<V>>;

         std::ranges::iterator_t<V> current_;
         transform_maybe_view* parent_;
         std::remove_cvref_t<std::invoke_result_t<F, std::ranges::range_reference_t<V>>> cache_;

         cursor() = default;
         constexpr cursor(transform_maybe_view* parent)
            : current_(parent->get_begin()), parent_(parent) {
            cache_ = std::invoke(parent_->func_, *current_);
         }
         constexpr cursor(as_sentinel_t, transform_maybe_view* parent)
            : current_(std::ranges::end(parent->base_)), parent_(parent) {}

         auto const& read() const {
            return *cache_;
         }

         //Walk forward until we find end, or element which returns an engaged optional and cache the result
         void next() {
            decltype(cache_) result = std::nullopt;
            do {
               ++current_;
               if (current_ == std::ranges::end(parent_->base_)) return;
               result = std::invoke(parent_->func_, *current_);
            } while (!result);
            cache_ = std::move(result);
         }

         //Walk backwards until we find begin, or element which returns an engaged optional and cache the result
         void prev() {
            decltype(cache_) result = std::nullopt;
            do {
               --current_;
               if (current_ == parent_->get_begin()) return;
               result = std::invoke(parent_->func_, *current_);
            } while (!result);
            cache_ = std::move(result);
         }

         bool equal(cursor const& s) const {
            return current_ == s.current_;
         }

         bool equal(sentinel const& s) const {
            return current_ == s.end_;
         }
      };
   public:
      template <class T>
      constexpr inline static bool am_common = std::ranges::common_range<T>;

      transform_maybe_view() = default;
      transform_maybe_view(V v, F f) : base_(std::move(v)), func_(std::move(f)) {}

      constexpr auto begin()  {
         return basic_iterator{ cursor(this) };
      }

      constexpr auto end() requires(am_common<V>) {
         //If the underlying range is bidirectional, then cursor::prev needs to get begin.
         //To make sure this is constant time, we should cache begin on the call to end.
         if constexpr (std::ranges::bidirectional_range<V>) get_begin();
         return basic_iterator{ cursor(as_sentinel, this) };
      }
      constexpr auto end() requires (!am_common<V>) {
         //If the underlying range is not common then we don't have to cache begin here,
         //because you won't be able to decrement the sentinel.
         return sentinel{std::ranges::end(base_)};
      }
   };

   template <class R, class F>
   transform_maybe_view(R&&, F f)->transform_maybe_view<std::views::all_t<R>, F>;

   namespace views {
      namespace detail {
         struct transform_maybe_fn_base {
            template <std::ranges::viewable_range R, class F>
            constexpr auto operator()(R&& r, F f) const 
            requires (std::ranges::input_range<R>, std::invocable<F, std::ranges::range_reference_t<R>>) {
               return transform_maybe_view(std::forward<R>(r), std::move(f));
            }
         };

         struct transform_maybe_fn : transform_maybe_fn_base {
            using transform_maybe_fn_base::operator(); 

            template <class F>
            constexpr auto operator()(F f) const {
               return pipeable(bind_back(transform_maybe_fn_base{}, std::move(f)));
            }
         };
      }

      constexpr inline detail::transform_maybe_fn transform_maybe;
   }
}

#endif