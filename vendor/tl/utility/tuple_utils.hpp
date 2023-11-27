#ifndef TL_UTILITY_TUPLE_UTILS_HPP
#define TL_UTILITY_TUPLE_UTILS_HPP

#include <type_traits>
#include <tuple>
#include <utility>
#include <functional>
#include <algorithm>
#include "../utility/meta.hpp"

namespace tl {
   //If the size of Ts is 2, returns pair<Ts...>, otherwise returns tuple<Ts...>
   namespace detail {
      template<class... Ts>
      struct tuple_or_pair_impl : std::type_identity<std::tuple<Ts...>> {};
      template<class Fst, class Snd>
      struct tuple_or_pair_impl<Fst, Snd> : std::type_identity<std::pair<Fst, Snd>> {};
   }
   template<class... Ts>
   using tuple_or_pair = typename detail::tuple_or_pair_impl<Ts...>::type;

   template <class Tuple>
   constexpr auto min_tuple(Tuple&& tuple) {
      return std::apply([](auto... sizes) {
         return std::ranges::min({
           std::common_type_t<decltype(sizes)...>(sizes)...
            });
         }, std::forward<Tuple>(tuple));
   }

   template <class Tuple>
   constexpr auto max_tuple(Tuple&& tuple) {
      return std::apply([](auto... sizes) {
         return std::ranges::max({
           std::common_type_t<decltype(sizes)...>(sizes)...
            });
         }, std::forward<Tuple>(tuple));
   }

   //Call f on every element of the tuple, returning a new one
   template<class F, class... Tuples>
   constexpr auto tuple_transform(F&& f, Tuples&&... tuples)
   {
      if constexpr (sizeof...(Tuples) > 1) {
         auto call_at_index = []<std::size_t Idx, class Fu, class... Ts>
            (tl::index_constant<Idx>, Fu f, Ts&&... tuples) {
            return f(std::get<Idx>(std::forward<Ts>(tuples))...);
         };

         constexpr auto min_size = tl::min_tuple(std::tuple(tl::tuple_size<Tuples>...));

         return[&] <std::size_t... Idx>(std::index_sequence<Idx...>) {
            return tuple_or_pair < std::decay_t<decltype(call_at_index(tl::index_constant<Idx>{}, std::move(f), std::forward<Tuples>(tuples)...)) > ... >
               (call_at_index(tl::index_constant<Idx>{}, std::move(f), std::forward<Tuples>(tuples)...)...);
         }(std::make_index_sequence<min_size>{});
      }
      else if constexpr (sizeof...(Tuples) == 1) {
         return std::apply([&]<class... Ts>(Ts&&... elements) {
            return tuple_or_pair<std::invoke_result_t<F&, Ts>...>(
               std::invoke(f, std::forward<Ts>(elements))...
               );
         }, std::forward<Tuples>(tuples)...);
      }
      else {
         return std::tuple{};
      }
   }

   //Call f on every element of the tuple
   template<class F, class Tuple>
   constexpr auto tuple_for_each(F&& f, Tuple&& tuple)
   {
      return std::apply([&]<class... Ts>(Ts&&... elements) {
         (std::invoke(f, std::forward<Ts>(elements)), ...);
      }, std::forward<Tuple>(tuple));
   }

   template <class Tuple>
   constexpr auto tuple_pop_front(Tuple&& tuple) {
      return std::apply([](auto&& head, auto&&... tail) {
         return std::pair(std::forward<decltype(head)>(head), std::tuple(std::forward<decltype(tail)>(tail)...));
         }, std::forward<Tuple>(tuple));
   }

   namespace detail {
      template <class F, class V>
      constexpr auto tuple_fold_impl(F, V v) {
         return v;
      }
      template <class F, class V, class Arg, class... Args>
      constexpr auto tuple_fold_impl(F f, V v, Arg arg, Args&&... args) {
         return tl::detail::tuple_fold_impl(f, 
            std::invoke(f, std::move(v), std::move(arg)), 
            std::forward<Args>(args)...);
      }
   }

   template <class F, class T, class Tuple>
   constexpr auto tuple_fold(Tuple tuple, T t, F f) {
      return std::apply([&](auto&&... args) {
         return tl::detail::tuple_fold_impl(std::move(f), std::move(t), std::forward<decltype(args)>(args)...);
         }, std::forward<Tuple>(tuple));
   }

   template<class... Tuples>
   constexpr auto tuple_zip(Tuples&&... tuples) {
      auto zip_at_index = []<std::size_t Idx, class... Ts>
         (tl::index_constant<Idx>, Ts&&... tuples) {
         return tuple_or_pair<std::decay_t<decltype(std::get<Idx>(std::forward<Ts>(tuples)))>...>(std::get<Idx>(std::forward<Ts>(tuples))...);
      };

      constexpr auto min_size = tl::min_tuple(std::tuple(tl::tuple_size<Tuples>...));

      return[&] <std::size_t... Idx>(std::index_sequence<Idx...>) {
         return tuple_or_pair<std::decay_t<decltype(zip_at_index(tl::index_constant<Idx>{}, std::forward<Tuples>(tuples)...))>... >
            (zip_at_index(tl::index_constant<Idx>{}, std::forward<Tuples>(tuples)...)...);
      }(std::make_index_sequence<min_size>{});
   }
}
#endif