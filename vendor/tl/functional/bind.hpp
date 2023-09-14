#ifndef TL_RANGES_FUNCTIONAL_BIND_HPP
#define TL_RANGES_FUNCTIONAL_BIND_HPP

#include <functional>

//tl::bind_back binds the last N arguments of f to the given ones, returning a new closure

namespace tl {
   template <class F, class... Args>
   constexpr auto bind_back(F&& f, Args&&... args) {
      return[f_ = std::forward<F>(f), ...args_ = std::forward<Args>(args)]
      (auto&&... other_args) 
      requires std::invocable<F&, decltype(other_args)..., Args&...> {
         return std::invoke(f_, std::forward<decltype(other_args)>(other_args)..., args_...);
      };
   }
}

#endif
