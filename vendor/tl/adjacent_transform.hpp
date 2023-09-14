#ifndef TL_RANGES_ADJACENT_TRANSFORM_HPP
#define TL_RANGES_ADJACENT_TRANSFORM_HPP

#include <ranges>
#include <type_traits>
#include "utility/semiregular_box.hpp"
#include "adjacent.hpp"
#include "common.hpp"
#include "utility/meta.hpp"

namespace tl {
   namespace detail {
      template<class T> using with_reference = T&;
      template<class T> concept can_reference
         = requires { typename with_reference<T>; };

      template<class F, class... Ts> struct regular_invocable_impl {
         static constexpr bool value = std::regular_invocable<F, Ts...>;
      };
   }

   template<std::ranges::forward_range V, std::copy_constructible F, std::size_t N>
   requires std::ranges::view<V> && (N > 0) && std::is_object_v<F> &&
      meta::repeat_into<std::ranges::range_reference_t<V>, N, meta::partial<detail::regular_invocable_impl, F&>::template apply>::type::value&&
      detail::can_reference<typename meta::repeat_into<std::ranges::range_reference_t<V>, N, meta::partial<std::invoke_result, F&>::template apply>::type::type>
      class adjacent_transform_view : public std::ranges::view_interface<adjacent_transform_view<V, F, N>> {
      //Might need to wrap F in a semiregular_box to ensure the view is moveable and default-initializable
      [[no_unique_address]] semiregular_storage_for<F> fun_;
      adjacent_view<V,N> inner_;

      using InnerView = adjacent_view<V,N>;
      template<bool Const>
      using inner_iterator = std::ranges::iterator_t<maybe_const<Const, InnerView>>;
      template<bool Const>
      using inner_sentinel = std::ranges::sentinel_t<maybe_const<Const, InnerView>>;

      static constexpr bool const_invocable = meta::repeat_into<
         std::ranges::range_reference_t<const V>, N, meta::partial<detail::regular_invocable_impl, const F&>::template apply
      >::type::value;

      template<bool Const>
      class sentinel {
         inner_sentinel<Const> inner_;
         constexpr explicit sentinel(inner_sentinel<Const> inner) : inner_(std::move(inner)) {}
      public:
         sentinel() = default;
         constexpr sentinel(sentinel<!Const> i)
            requires Const&& std::convertible_to<inner_sentinel<false>, inner_sentinel<Const>> : inner_(std::move(i.inner_)) {}
      };

      template<bool Const>
      class cursor {
         using Parent = maybe_const<Const, adjacent_transform_view>;
         using Base = maybe_const<Const, V>;
         Parent* parent_ = nullptr;
         inner_iterator<Const> inner_ = inner_iterator<Const>();

      public:
         using value_type =
            std::remove_cvref_t<
               typename meta::repeat_into<std::ranges::range_reference_t<Base>, N, 
                                    meta::partial<std::invoke_result, maybe_const<Const, F>&>::template apply
               >::type::type>;

         constexpr cursor(Parent& parent, inner_iterator<Const> inner) :
            parent_(std::addressof(parent)), inner_(std::move(inner)) {}

         cursor() = default;
         constexpr cursor(cursor<!Const> i)
            requires Const&& std::convertible_to<inner_iterator<false>, inner_iterator<Const>>
            : parent_(i.parent_), inner_(std::move(i.inner_)) {}

         //TODO noexcept
         constexpr decltype(auto) read() const {
             return[i = inner_.get().first_,this]<std::size_t... Indices>(std::index_sequence<Indices...>) mutable {
                 return std::invoke(parent_->fun_, ((void)Indices, *i++)...);
             }(std::make_index_sequence<N>{});
         }

         constexpr void next() {
            ++inner_;
         }

         constexpr void prev()
            requires std::ranges::bidirectional_range<Base> {
            --inner_;
         }

         constexpr void advance(std::ranges::range_difference_t<Base> n)
            requires std::ranges::random_access_range<Base> {
            inner_ += n;
         }

         constexpr bool equal(cursor const& rhs) const
            requires std::equality_comparable<inner_iterator<Const>> {
            return inner_ == rhs.inner_;
         }

         constexpr bool equal(sentinel<Const> const& rhs) const
            requires std::sentinel_for<inner_sentinel<Const>, inner_iterator<Const>>{
            return inner_ == rhs.end_;
         }

         constexpr auto distance_to(cursor const& rhs) const
            requires std::sized_sentinel_for<inner_iterator<Const>, inner_iterator<Const>> {
            return rhs.inner_ - inner_;
         }

         constexpr auto distance_to(sentinel<Const> const& rhs) const
            requires std::sized_sentinel_for<inner_sentinel<Const>, inner_iterator<Const>> {
            return rhs.inner_ - inner_;
         }
      };

      public:
         constexpr adjacent_transform_view() = default;

         constexpr explicit adjacent_transform_view(F fun, V view) : fun_(std::move(fun)), inner_(std::move(view)) {}

         constexpr auto begin() {
            return basic_iterator{ cursor<false>(*this, inner_.begin()) };
         }

         constexpr auto begin() const
            requires std::ranges::range<const InnerView>&& const_invocable           
         {
            return basic_iterator{ cursor<true>(*this, inner_.begin()) };
         }

         constexpr auto end() {
            return sentinel<false>(inner_.end());
         }

         constexpr auto end() requires std::ranges::common_range<InnerView> {
            return basic_iterator{ cursor<false>(*this, inner_.end()) };
         }

         constexpr auto end() const
            requires std::ranges::range<const InnerView>&& const_invocable
         {
            return sentinel<true>(inner_.end());
         }

         constexpr auto end() const
            requires std::ranges::common_range<const InnerView>&&
            const_invocable
         {
            return basic_iterator{ cursor<true>(*this, inner_.end()) };
         }

         constexpr auto size() requires std::ranges::sized_range<InnerView> {
            return inner_.size();
         }

         constexpr auto size() const requires std::ranges::sized_range<const InnerView> {
            return inner_.size();
         }
   };

   namespace views {
      namespace detail {
         template <std::size_t N>
         class adjacent_transform_fn_base {
         public:
            template <std::ranges::viewable_range V, std::copy_constructible F>
            requires meta::repeat_into<std::ranges::range_reference_t<V>, N, meta::partial<tl::detail::regular_invocable_impl, F&>::template apply>::type::value&&
               tl::detail::can_reference<typename meta::repeat_into<std::ranges::range_reference_t<V>, N, meta::partial<std::invoke_result, F&>::template apply>::type::type>
               constexpr auto operator()(V&& v, F f) const {
               return tl::adjacent_transform_view<std::views::all_t<V>, F, N>{ std::move(f), std::views::all(std::forward<V>(v)) };
            }
         };

         template <std::size_t N>
         struct adjacent_transform_fn : adjacent_transform_fn_base<N> {
            using adjacent_transform_fn_base<N>::operator();

            template <class F>
            constexpr auto operator()(F f) const {
               return pipeable(bind_back(adjacent_transform_fn_base<N>{}, std::move(f)));
            }
         };
      }  // namespace detail

 
      template <std::size_t N>
      inline constexpr auto adjacent_transform = detail::adjacent_transform_fn<N>{};
      inline constexpr auto pairwise_transform = detail::adjacent_transform_fn<2>{};
   }  // namespace views

}

#endif