#ifndef TL_RANGES_FUNCTIONAL_PIPEABLE_HPP
#define TL_RANGES_FUNCTIONAL_PIPEABLE_HPP

#include <type_traits>
#include <utility>
#include "compose.hpp"

//tl::pipeable takes some invocable and enables:
//- Piping a single argument to it such that a | pipeable is the same as pipeable(a)
//- Piping it to another pipeable object, such that a | b is the same as tl::compose(b, a)

namespace tl {
   struct pipeable_base {};
   template <class T>
   concept is_pipeable = std::is_base_of_v<pipeable_base, std::remove_cvref_t<T>>;

   template <class F>
   struct pipeable_fn : pipeable_base {
      [[no_unique_address]] F f_;

      constexpr pipeable_fn(F f) : f_(std::move(f)) {}

      template <class... Args>
      constexpr auto operator()(Args&&... args) const requires std::invocable<F, Args...> {
         return std::invoke(f_, std::forward<Args>(args)...);
      }
   };

   template <class F>
   constexpr auto pipeable(F f) {
      return pipeable_fn{ std::move(f) };
   }

   template <class V, class Pipe>
   constexpr auto operator|(V&& v, Pipe&& fn)
      requires (!is_pipeable<V> && is_pipeable<Pipe> && std::invocable<Pipe, V>) {
      return std::invoke(std::forward<Pipe>(fn).f_, std::forward<V>(v));
   }

   template <class Pipe1, class Pipe2>
   constexpr auto operator|(Pipe1&& p1, Pipe2&& p2)
      requires (is_pipeable<Pipe1>&& is_pipeable<Pipe2>) {
      return pipeable(compose(std::forward<Pipe2>(p2).f_, std::forward<Pipe1>(p1).f_));
   }
}

#endif