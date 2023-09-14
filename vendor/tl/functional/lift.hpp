#ifndef TL_RANGES_FUNCTIONAL_LIFT_HPP
#define TL_RANGES_FUNCTIONAL_LIFT_HPP

#include <functional>
#define TL_LIFT(func) \
   [](auto&&... args) \
      noexcept(noexcept(func(std::forward<decltype(args)>(args)...)))\
      ->decltype (func(std::forward<decltype(args)>(args)...)) \
   {\
      return func(std::forward<decltype(args)>(args)...);\
   }

#endif