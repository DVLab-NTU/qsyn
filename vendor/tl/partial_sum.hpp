#ifndef TL_RANGES_PARTIAL_SUM_HPP
#define TL_RANGES_PARTIAL_SUM_HPP

#include <iterator>
#include <ranges>
#include <type_traits>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"
#include "utility/semiregular_box.hpp"
#include "utility/non_propagating_cache.hpp"
#include "functional/bind.hpp"

namespace tl {
   template <std::ranges::input_range V, std::invocable<std::ranges::range_value_t<V>&, std::ranges::range_reference_t<V>> F>
   requires std::ranges::view<V> class partial_sum_view
      : public std::ranges::view_interface<partial_sum_view<V, F>> {
   private:
      V base_;
      [[no_unique_address]] semiregular_storage_for<F> func_;
      mutable non_propagating_cache<std::ranges::range_value_t<V>> cache_;

      template <bool Const>
      class cursor {
         using Base = std::conditional_t<Const, const V, V>;

         std::ranges::iterator_t<Base> current_{};
         maybe_const<Const, partial_sum_view>* parent_;

      public:
         static constexpr bool single_pass = detail::single_pass_iterator<std::ranges::iterator_t<Base>>;

         cursor() = default;
         constexpr explicit cursor(std::ranges::iterator_t<Base> current, maybe_const<Const, partial_sum_view>* parent)
            : current_{ std::move(current) }, parent_{ parent } {
            parent->cache_.emplace(*current);
         }

         constexpr cursor(cursor<!Const> i) requires Const&& std::convertible_to<
            std::ranges::iterator_t<V>,
            std::ranges::iterator_t<Base>>
            : current_{ std::move(i.current_) }, parent_{ i.parent_ } {}

         constexpr decltype(auto) read() const {
            return parent_->cache_.value();
         }

         constexpr void next() {
            ++current_;
            if (current_ != std::ranges::end(parent_->base_)) {
               parent_->cache_ = std::invoke(parent_->func_, parent_->cache_.value(), *current_);
            }
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
      partial_sum_view() = default;
      partial_sum_view(V base, F func) : base_(std::move(base)), func_(std::move(func)) {}

      constexpr auto begin() requires(!simple_view<V>) {
         return basic_iterator{ cursor<false>(std::ranges::begin(base_), this) };
      }
      constexpr auto begin() const requires std::ranges::range<const V> {
         return basic_iterator{ cursor<true>(std::ranges::begin(base_), this) };
      }

      constexpr auto end() requires(!simple_view<V>) {
         return basic_sentinel<V, false>{std::ranges::end(base_)};
      }

      constexpr auto end() const requires (std::ranges::range<const V>) {
         return basic_sentinel<V, true>{std::ranges::end(base_)};
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

   namespace views {
      namespace detail {
         struct partial_sum_fn_base {
            template <std::ranges::viewable_range R, std::invocable<std::ranges::range_value_t<R>&, std::ranges::range_reference_t<R>> F = std::plus<>>
            constexpr auto operator()(R&& r, F f = {}) const
               requires std::ranges::input_range<R> {
               return partial_sum_view<std::views::all_t<R>, F>(std::views::all(std::forward<R>(r)), std::move(f));
            }
         };

         struct partial_sum_fn : partial_sum_fn_base {
            using partial_sum_fn_base::operator();

            template <class F = std::plus<>>
            constexpr auto operator()(F f = {}) const {
               return pipeable(tl::bind_back(partial_sum_fn_base{}, std::move(f)));
            }
         };
      }

      constexpr inline auto partial_sum = detail::partial_sum_fn{};
   }  // namespace views
}  // namespace tl

#endif