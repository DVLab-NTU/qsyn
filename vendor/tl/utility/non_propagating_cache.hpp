#ifndef TL_RANGES_UTILITY_NON_PROPAGATING_CACHE_HPP
#define TL_RANGES_UTILITY_NON_PROPAGATING_CACHE_HPP

// A non-propagating cache is like a std::optional, but if you copy it 
// then the copy is invalidated, and if you move it, both the original
// and the moved version are invalidated.
// 
// The idea is that a copy/move of some view should not depend
// on local cache.
// 
// See http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2328r0.html

#include <optional>

namespace tl {
   template <class T>
   requires (std::is_object_v<T>)
   class non_propagating_cache : public std::optional<T> {
   public:
      using std::optional<T>::operator=;
      non_propagating_cache() = default;
      constexpr non_propagating_cache(std::nullopt_t) noexcept : std::optional<T>{} {}
      constexpr non_propagating_cache(non_propagating_cache const&) noexcept : std::optional<T>{} { }
      constexpr non_propagating_cache(non_propagating_cache && other) noexcept : std::optional<T>{} {
         other.reset();
      }
      constexpr non_propagating_cache& operator=(non_propagating_cache const& other) noexcept {
         if (std::addressof(other) != this)
            this->reset();
         return *this;
      }
      constexpr non_propagating_cache& operator=(non_propagating_cache && other) noexcept {
         this->reset();
         other.reset();
         return *this;
      }
   };
}

#endif