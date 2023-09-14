#ifndef TL_RANGES_SEMIREGULAR_BOX_HPP
#define TL_RANGES_SEMIREGULAR_BOX_HPP

#include <concepts>
#include <optional>

namespace tl {
   //A semiregular box is a wrapper used for storing non-default-initializable or assignable types in views.
   //It's commonly used for storing invocables.
   //It's pretty much the same as std::optional, but default-initializing it will default-initialize a T
   //rather than leaving the box empty, and assignment can also be done by emplacing rather than assigning
   //the stored T.
   template<std::copy_constructible T>
   requires std::is_object_v<T>
      class semiregular_box : public std::optional<T> {
      public:
         using std::optional<T>::optional;
         using std::optional<T>::operator*;

         constexpr semiregular_box() noexcept(std::is_nothrow_default_constructible_v<T>)
            requires std::default_initializable<T>
            : std::optional<T>{ std::in_place }
         { }

         semiregular_box(semiregular_box const&) = default;
         semiregular_box(semiregular_box&&) = default;

         using std::optional<T>::operator=;

         semiregular_box& operator=(semiregular_box const& rhs)
            noexcept(std::is_nothrow_copy_constructible_v<T>)
            requires (!std::assignable_from<T&, T const&>) {
            if (rhs) {
               this->emplace(*rhs);
            }
            else {
               this->reset();
            }
            return *this;
         }

         semiregular_box& operator=(semiregular_box&& rhs)
            noexcept(std::is_nothrow_move_constructible_v<T>)
            requires (!std::assignable_from<T&, T>)
         {
            if (rhs) {
               this->emplace(std::move(*rhs));
            }
            else {
               this->reset();
            }
            return *this;
         }

         template <class... Args>
         constexpr decltype(auto) operator()(Args&&... args)
            requires std::invocable<T&, Args...> {
            return std::invoke(this->value(), std::forward<Args>(args)...);
         }

         template <class... Args>
         constexpr decltype(auto) operator()(Args&&... args) const
            requires std::invocable<const T&, Args...> {
            return std::invoke(this->value(), std::forward<Args>(args)...);
         }
   };

   template <class T>
   using semiregular_storage_for = std::conditional_t<std::semiregular<T>, T, semiregular_box<T>>;
}
#endif