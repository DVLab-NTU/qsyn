#ifndef TL_RANGES_TRANSFORM_JOIN_HPP
#define TL_RANGES_TRANSFORM_JOIN_HPP

#include <ranges>
#include <utility>
#include "functional/pipeable.hpp"
#include "functional/bind.hpp"
#include "cache_latest.hpp"

namespace tl {
   namespace views {
      namespace detail {
         template <class V, class F>
         concept transformable_view = requires(V && v, F f) {
            std::ranges::transform_view{ std::forward<V>(v), std::move(f) };
         };

         template <class V>
         concept joinable_view = requires(V && v) {
            std::ranges::join_view{ std::forward<V>(v) };
         };

         struct transform_join_fn_base {
            template <std::ranges::viewable_range V, class F>
            constexpr auto operator()(V&& v, F f) const
               requires (transformable_view<V, F> &&
                  (joinable_view<std::ranges::transform_view<std::views::all_t<V>, F>> ||
                     joinable_view<tl::cache_latest_view<std::ranges::transform_view<std::views::all_t<V>, F>>>)) {
               if constexpr (joinable_view<std::ranges::transform_view<std::views::all_t<V>, F>>) {
                  return v | std::views::transform(f) | std::views::join;
               }
               else {
                  return v | std::views::transform(f) | tl::views::cache_latest | std::views::join;
               }
            }
         };
            
         struct transform_join_fn : transform_join_fn_base {
            using transform_join_fn_base::operator();

            template <class F>
            constexpr auto operator()(F f) const {
               return pipeable(bind_back(transform_join_fn_base{}, std::move(f)));
            }
         };
      }

      constexpr inline auto transform_join = detail::transform_join_fn{};
   }
}

#endif 