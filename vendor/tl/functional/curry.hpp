#ifndef TL_RANGES_FUNCTIONAL_CURRY_HPP
#define TL_RANGES_FUNCTIONAL_CURRY_HPP

#include <utility>
#include <functional>

//tl::uncurry adapts a function which takes some number of arguments into
//one which takes its arguments from a tuple.

//tl::curry is the opposite.

namespace tl {
   template <class F>
   auto uncurry(F f) {
      return[f_ = std::move(f)](auto&& tuple) {
         return std::apply(f_, std::forward<decltype(tuple)>(tuple));
      };
   }

   template <class F>
   auto curry(F f) {
      return[f_ = std::move(f)](auto&&... args) {
         return std::invoke(f_, std::tuple(std::forward<decltype(args)>(args)...));
      };
   }
}

#endif