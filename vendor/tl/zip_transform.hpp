#ifndef TL_RANGES_ZIP_TRANSFORM_HPP
#define TL_RANGES_ZIP_TRANSFORM_HPP

#include <ranges>
#include <type_traits>
#include "utility/semiregular_box.hpp"
#include "zip.hpp"
#include "common.hpp"

namespace tl {
   namespace detail {
      template<class T> using with_reference = T&;
      template<class T> concept can_reference
         = requires { typename with_reference<T>; };
   }

   template<std::copy_constructible F, std::ranges::input_range... Views>
   requires (std::ranges::view<Views> && ...) && (sizeof...(Views) > 0) && std::is_object_v<F>&&
      std::regular_invocable<F&, std::ranges::range_reference_t<Views>...>&&
      detail::can_reference<std::invoke_result_t<F&, std::ranges::range_reference_t<Views>...>>
      class zip_transform_view : public std::ranges::view_interface<zip_transform_view<F, Views...>> {
      //Might need to wrap F in a semiregular_box to ensure the view is moveable and default-initializable
      [[no_unique_address]] semiregular_storage_for<F> fun_;
      zip_view<Views...> zip_;

      using InnerView = zip_view<Views...>;
      template<bool Const>
      using ziperator = std::ranges::iterator_t<maybe_const<Const, InnerView>>;
      template<bool Const>
      using zentinel = std::ranges::sentinel_t<maybe_const<Const, InnerView>>;

      template<bool Const>
      class sentinel {
         zentinel<Const> inner_;
         constexpr explicit sentinel(zentinel<Const> inner) : inner_(std::move(inner)) {}
      public:
         sentinel() = default;
         constexpr sentinel(sentinel<!Const> i)
            requires Const&& std::convertible_to<zentinel<false>, zentinel<Const>> : inner_(std::move(i.inner_)) {}
      };

      template<bool Const>
      class cursor {
         using Parent = maybe_const<Const, zip_transform_view>;
         using Base = maybe_const<Const, InnerView>;
         Parent* parent_ = nullptr;
         ziperator<Const> inner_ = ziperator<Const>();

      public:
         using value_type = 
            std::remove_cvref_t<std::invoke_result_t<maybe_const<Const, F>&, 
                                std::ranges::range_reference_t<maybe_const<Const, Views>>...>>;

         constexpr cursor(Parent& parent, ziperator<Const> inner) :
            parent_(std::addressof(parent)), inner_(std::move(inner)) {}

         cursor() = default;
         constexpr cursor(cursor<!Const> i)
            requires Const&& std::convertible_to<ziperator<false>, ziperator<Const>>
            : parent_(i.parent_), inner_(std::move(i.inner_)) {}

         //TODO noexcept
         constexpr decltype(auto) read() const {
            return std::apply([&](const auto&... iters) -> decltype(auto) {
               return std::invoke(parent_->fun_, *iters...);
               }, inner_.get().currents_);
         };

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
            requires std::equality_comparable<ziperator<Const>> {
            return inner_ == rhs.inner_;
         }

         constexpr bool equal(sentinel<Const> const& rhs) const
            requires std::sentinel_for<zentinel<Const>, ziperator<Const>>{
            return inner_ == rhs.end_;
         }

         constexpr auto distance_to(cursor const& rhs) const
            requires std::sized_sentinel_for<ziperator<Const>, ziperator<Const>> {
            return rhs.inner_ - inner_;
         }

         constexpr auto distance_to(sentinel<Const> const& rhs) const
            requires std::sized_sentinel_for<zentinel<Const>, ziperator<Const>> {
            return rhs.inner_ - inner_;
         }
      };

      public:
         constexpr zip_transform_view() = default;

         constexpr explicit zip_transform_view(F fun, Views... views) : fun_(std::move(fun)), zip_(std::move(views)...) {}

         constexpr auto begin() {
            return basic_iterator{ cursor<false>(*this, zip_.begin()) };
         }

         constexpr auto begin() const
            requires std::ranges::range<const InnerView>&&
            std::regular_invocable<const F&, std::ranges::range_reference_t<const Views>...>
         {
            return basic_iterator{ cursor<true>(*this, zip_.begin()) };
         }

         constexpr auto end() {
            return sentinel<false>(zip_.end());
         }

         constexpr auto end() requires std::ranges::common_range<InnerView> {
            return basic_iterator{ cursor<false>(*this, zip_.end()) };
         }

         constexpr auto end() const
            requires std::ranges::range<const InnerView>&&
            std::regular_invocable<const F&, std::ranges::range_reference_t<const Views>...>
         {
            return sentinel<true>(zip_.end());
         }

         constexpr auto end() const
            requires std::ranges::common_range<const InnerView>&&
            std::regular_invocable<const F&, std::ranges::range_reference_t<const Views>...>
         {
            return basic_iterator{ cursor<true>(*this, zip_.end()) };
         }

         constexpr auto size() requires std::ranges::sized_range<InnerView> {
            return zip_.size();
         }

         constexpr auto size() const requires std::ranges::sized_range<const InnerView> {
            return zip_.size();
         }
   };

   template<class F, class... Rs>
   zip_transform_view(F, Rs&&...)->zip_transform_view<F, std::views::all_t<Rs>...>;

   namespace views {
      namespace detail {
         class zip_transform_fn {
         public:
            constexpr std::ranges::empty_view<std::tuple<>> operator()() const noexcept {
               return {};
            }
            template <std::copy_constructible F, std::ranges::viewable_range... V>
            requires ((std::ranges::input_range<V> && ...) && (sizeof...(V) != 0)) && std::is_object_v<F>&&
               std::regular_invocable<F&, std::ranges::range_reference_t<V>...>&&
               tl::detail::can_reference<std::invoke_result_t<F&, std::ranges::range_reference_t<V>...>>
               constexpr auto operator()(F f, V&&... vs) const {
               return tl::zip_transform_view{ std::move(f), std::views::all(std::forward<V>(vs))... };
            }
         };
      }  // namespace detail

      inline constexpr detail::zip_transform_fn zip_transform;
   }  // namespace views

}

#endif