#ifndef TL_UTILITY_META_HPP
#define TL_UTILITY_META_HPP

#include <type_traits>
#include <tuple>
#include <utility>

namespace tl {
   template <class Tuple>
   constexpr inline std::size_t tuple_size = std::tuple_size_v<std::remove_cvref_t<Tuple>>;

   template <std::size_t N>
   using index_constant = std::integral_constant<std::size_t, N>;

   namespace meta {
      //Partially-apply the given template with the given arguments
      template <template <class...> class T, class... Args>
      struct partial {
         template <class... MoreArgs>
         struct apply {
            using type = T<Args..., MoreArgs...>;
         };
      };

      namespace detail {
         template <class T, template<class...> class Into, std::size_t... Idx>
         constexpr auto repeat_into_impl(std::index_sequence<Idx...>)
            ->Into < typename decltype((Idx, std::type_identity<T>{}))::type... > ;
      }

      //Repeat the given type T into the template Into N times
      template <class T, std::size_t N, template <class...> class Into>
      using repeat_into = decltype(tl::meta::detail::repeat_into_impl<T,Into>(std::make_index_sequence<N>{}));
   }
}

#endif