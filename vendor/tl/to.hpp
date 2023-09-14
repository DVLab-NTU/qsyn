#ifndef TL_RANGES_TO_HPP
#define TL_RANGES_TO_HPP

#include <ranges>
#include <iterator>
#include <algorithm>

namespace tl {
   namespace detail {
      template <class C, class R>
      concept reservable = std::ranges::sized_range<R> && requires(C & c, R && rng) {
         { c.capacity() } -> std::same_as<std::ranges::range_size_t<C>>;
         { c.reserve(std::ranges::range_size_t<R>(0)) };
      };

      template <class C>
      concept insertable = requires(C c) {
         std::inserter(c, std::ranges::end(c));
      };

      template <class>
      constexpr inline bool always_false = false;

      //R is a nested range that can be converted to the nested container C
      template <class C, class R>
      concept matroshkable = std::ranges::input_range<C> && std::ranges::input_range<R> &&
         std::ranges::input_range<std::ranges::range_value_t<C>> &&
         std::ranges::input_range<std::ranges::range_value_t<R>> &&
         !std::ranges::view<std::ranges::range_value_t<C>> &&
         std::indirectly_copyable<
         std::ranges::iterator_t<std::ranges::range_value_t<R>>,
         std::ranges::iterator_t<std::ranges::range_value_t<C>>
         > &&
         detail::insertable<C>;

      template <std::ranges::input_range R>
      struct fake_input_iterator {
         using iterator_category = std::input_iterator_tag;
         using value_type = std::ranges::range_value_t<R>;
         using difference_type = std::ranges::range_difference_t<R >;
         using pointer = std::ranges::range_value_t<R>*;
         using reference = std::ranges::range_reference_t<R >;
         reference operator*();
         fake_input_iterator& operator++();
         fake_input_iterator operator++(int);
         fake_input_iterator() = default;
         friend bool operator==(fake_input_iterator a, fake_input_iterator b) { return false; }
      };

      template <template <typename...> typename C, std::ranges::input_range R, typename... Args>
      struct ctad_container {
         template <class V = R>
         static auto deduce(int) -> decltype(C(std::declval<V>(), std::declval<Args>()...));

         template <class Iter = fake_input_iterator<R>>
         static auto deduce(...) -> decltype(C(std::declval<Iter>(), std::declval<Iter>(), std::declval<Args>()...));

         using type = decltype(deduce(0));
      };
   }

   template <std::ranges::input_range C, std::ranges::input_range R, typename... Args>
   requires (!std::ranges::view<C>)
      constexpr C to(R&& r, Args&&... args) {
      //Construct from range
      if constexpr (std::constructible_from<C, R, Args...>) {
         return C(std::forward<R>(r), std::forward<Args>(args)...);
      }
      //Construct and copy (potentially reserving memory)
      else if constexpr (std::constructible_from<C, Args...> && std::indirectly_copyable<std::ranges::iterator_t<R>, std::ranges::iterator_t<C>> && detail::insertable<C>) {
         C c(std::forward<Args>(args)...);
         if constexpr (std::ranges::sized_range<R> && detail::reservable<C, R>) {
            c.reserve(std::ranges::size(r));
         }
         std::ranges::copy(r, std::inserter(c, std::end(c)));
         return c;
      }
      //Nested case
      else if constexpr (detail::matroshkable<C, R>) {
         C c(std::forward<Args...>(args)...);
         if constexpr (std::ranges::sized_range<R> && detail::reservable<C, R>) {
            c.reserve(std::ranges::size(r));
         }
         auto v = r | std::views::transform([](auto&& elem) {
            return tl::to<std::ranges::range_value_t<C>>(elem);
            });
         std::ranges::copy(v, std::inserter(c, std::end(c)));
         return c;
      }
      //Construct from iterators
      else if constexpr (std::constructible_from<C, std::ranges::iterator_t<R>, std::ranges::sentinel_t<R>, Args...>) {
         return C(std::ranges::begin(r), std::ranges::end(r), std::forward<Args>(args)...);
      }
      else {
         static_assert(detail::always_false<C>, "C is not constructible from R");
      }
   }

   template <template <typename...> typename C, std::ranges::input_range R, typename... Args, class ContainerType = typename detail::ctad_container<C, R, Args...>::type>
   constexpr auto to(R&& r, Args&&... args) -> ContainerType {
      return tl::to<ContainerType>(std::forward<R>(r), std::forward<Args>(args)...);
   }

   namespace detail {
      template <std::ranges::input_range C, class... Args>
      struct closure_range {
         template <class... A>
         closure_range(A&&... as) :args_(std::forward<A>(as)...) {}
         std::tuple<Args...> args_;
      };
      template <std::ranges::input_range R, std::ranges::input_range C, class... Args>
      auto constexpr operator| (R&& r, closure_range<C, Args...>&& c) {
         return std::apply([&r](auto&&... inner_args) {
            return tl::to<C>(std::forward<R>(r), std::forward<decltype(inner_args)>(inner_args)...);
            }, std::move(c.args_));
      }
      template <template <class...> class C, class... Args>
      struct closure_ctad {
         template <class... A>
         closure_ctad(A&&... as) :args_(std::forward<A>(as)...) {}
         std::tuple<Args...> args_;
      };
      template <std::ranges::input_range R, template <class...> class C, class... Args>
      auto constexpr operator| (R&& r, closure_ctad<C, Args...>&& c) {
         return std::apply([&r](auto&&... inner_args) {
            return tl::to<C>(std::forward<R>(r));
            }, std::move(c.args_));
      }
   }

   template <template <typename...> typename C, class... Args>
   constexpr auto to(Args&&... args) {
      return detail::closure_ctad<C, Args...>{ std::forward<Args>(args)... };
   }

   template <std::ranges::input_range C, class... Args>
   constexpr auto to(Args&&... args) {
      return detail::closure_range<C, Args...>{ std::forward<Args>(args)... };
   }
}

#endif
