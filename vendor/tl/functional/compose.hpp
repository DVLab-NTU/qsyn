#ifndef TL_RANGES_FUNCTIONAL_COMPOSE_HPP
#define TL_RANGES_FUNCTIONAL_COMPOSE_HPP

#include <functional>

//tl::compose composes f and g such that compose(f,g)(args...) is f(g(args...)), i.e. g is called first

namespace tl {
   template <class F, class G>
   struct compose_fn {
      [[no_unique_address]] F f;
      [[no_unique_address]] G g;

      template <class A, class B>
      compose_fn(A&& a, B&& b) : f(std::forward<A>(a)), g(std::forward<B>(b)) {}

      template <class A, class B, class ... Args>
      static constexpr auto call(A&& a, B&& b, Args&&... args) {
         if constexpr (std::is_void_v<std::invoke_result_t<G, Args...>>) {
            std::invoke(std::forward<B>(b), std::forward<Args>(args)...);
            return std::invoke(std::forward<A>(a));
         }
         else {
            return std::invoke(std::forward<A>(a), std::invoke(std::forward<B>(b), std::forward<Args>(args)...));
         }
      }

      template <class... Args>
      constexpr auto operator()(Args&&... args) & {
         return call(f, g, std::forward<Args>(args)...);
      }

      template <class... Args>
      constexpr auto operator()(Args&&... args) const& {
         return call(f, g, std::forward<Args>(args)...);
      }

      template <class... Args>
      constexpr auto operator()(Args&&... args)&& {
         return call(std::move(f), std::move(g), std::forward<Args>(args)...);
      }

      template <class... Args>
      constexpr auto operator()(Args&&... args) const&& {
         return call(std::move(f), std::move(g), std::forward<Args>(args)...);
      }
   };

   template <class F, class G>
   constexpr auto compose(F&& f, G&& g) {
      return compose_fn<std::remove_cvref_t<F>, std::remove_cvref_t<G>>(std::forward<F>(f), std::forward<G>(g));
   }
}

#endif 