#ifndef TL_RANGES_CHUNK_HPP
#define TL_RANGES_CHUNK_HPP

#include <ranges>
#include <iterator>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "functional/pipeable.hpp"
#include "functional/bind.hpp"

namespace tl {
   template <std::ranges::forward_range V>
   requires std::ranges::view<V> class chunk_view
      : public std::ranges::view_interface<chunk_view<V>> {
   private:
      //Cannot be common for non-sized bidirectional ranges because calculating the predecessor to end would be O(n).
      template <class T>
      static constexpr bool am_common = std::ranges::common_range<T> &&
         (!std::ranges::bidirectional_range<T> || std::ranges::sized_range<T>);

      template <class T> static constexpr bool am_sized = std::ranges::sized_range<T>;

      V base_;
      std::ranges::range_difference_t<V> chunk_size_;

      //The cursor for chunk_view may need to keep track of additional state.
      //Consider the case where you have a vector of 0,1,2 and you chunk with a size of 2.
      //If you take the end iterator for the chunk_view and decrement it, then you should end up at 2, not at 1.
      //As such, there's additional work required to track where to decrement to in the case that the underlying range is bidirectional.
      //This is handled by a bunch of base classes for the cursor.

      //Case when underlying range is not bidirectional, i.e. we don't care about calculating offsets
      template <bool Const, class Base = std::conditional_t<Const, const V, V>, bool = std::ranges::bidirectional_range<Base>, bool = std::ranges::sized_range<Base>>
      struct cursor_base {
         cursor_base() = default;
         constexpr explicit cursor_base(std::ranges::range_difference_t<Base> offs) {}

         //These are both no-ops
         void set_offset(std::ranges::range_difference_t<Base> off) {}
         std::ranges::range_difference_t<Base> get_offset() {
            return 0;
         }
      };

      //Case when underlying range is bidirectional but not sized. We need to keep track of the offset if we hit the end iterator.
      template <bool Const, class Base>
      struct cursor_base<Const, Base, true, false> {
         using difference_type = std::ranges::range_difference_t<Base>;
         difference_type offset_{};

         cursor_base() = default;
         constexpr explicit cursor_base(difference_type offset)
            : offset_{ offset }  {}

         void set_offset(difference_type off) {
            offset_ = off;
         }

         difference_type get_offset() {
            return offset_;
         }
      };

      //Case where underlying is bidirectional and sized. We can calculate offsets from the end on construction.
      template <bool Const, class Base>
      struct cursor_base<Const, Base, true, true> {
         using difference_type = std::ranges::range_difference_t<Base>;
         difference_type offset_{};

         cursor_base() = default;
         constexpr explicit cursor_base(difference_type offset)
            : offset_{ offset } {}

         //No-op because we're precomputing the offset
         void set_offset(std::ranges::range_difference_t<Base>) {}

         std::ranges::range_difference_t<Base> get_offset() {
            return offset_;
         }
      };

      template <bool Const>
      struct cursor : cursor_base<Const> {
         template <class T>
         using constify = std::conditional_t<Const, const T, T>;
         using Base = constify<V>;

         using difference_type = std::ranges::range_difference_t<Base>;

         std::ranges::iterator_t<Base> current_{};
         std::ranges::sentinel_t<Base> end_{};
         std::ranges::range_difference_t<Base> chunk_size_{};

         cursor() = default;
         
         //Pre-calculate the offset for sized ranges
         constexpr cursor(std::ranges::iterator_t<Base> begin, Base* base, std::ranges::range_difference_t<Base> chunk_size)
            requires(std::ranges::sized_range<Base>)
            : cursor_base<Const>(chunk_size - (std::ranges::size(*base) % chunk_size)),
            current_(std::move(begin)), end_(std::ranges::end(*base)), chunk_size_(chunk_size) {}

         constexpr cursor(std::ranges::iterator_t<Base> begin, Base* base, std::ranges::range_difference_t<Base> chunk_size)
            requires(!std::ranges::sized_range<Base>)
            : cursor_base<Const>(), current_(std::move(begin)), end_(std::ranges::end(*base)), chunk_size_(chunk_size) {}

         //const-converting constructor
         constexpr cursor(cursor<!Const> i) requires Const&& std::convertible_to<
            std::ranges::iterator_t<V>,
            std::ranges::iterator_t<const V>>
            : cursor_base<Const>(i.get_offset()) {}

         constexpr auto read() const {
            auto last = std::ranges::next(current_, chunk_size_, end_);
            return std::ranges::subrange{ current_, last };
         }

         constexpr void next() {
            auto delta = std::ranges::advance(current_, chunk_size_, end_);
            //This will track the amount we actually moved by last advance, 
            //which will be less than the chunk size if range_size % chunk_size != 0
            this->set_offset(delta);
         }

         constexpr void prev() requires std::ranges::bidirectional_range<Base> {
            auto delta = -chunk_size_;
            //If we're at the end we may need to offset the amount to move back by
            if (current_ == end_) {
               delta += this->get_offset();
            }
            std::advance(current_, delta);
         }

         constexpr void advance(difference_type x)
            requires std::ranges::random_access_range<Base> {
            if (x == 0) return;

            x *= chunk_size_;

            if (x > 0) {
               auto delta = std::advance(current_, x, end_);
               this->set_offset(delta);
            }
            else if (x < 0) {
               if (current_ == end_) {
                  x += this->get_offset();
               }
               std::advance(this->current, x);
            }
         }

         constexpr bool equal(cursor const& rhs) const {
            return current_ == rhs.current_;
         }
         constexpr bool equal(basic_sentinel<V,Const> const& rhs) const {
            return current_ == rhs.end_;
         }

         friend struct cursor<!Const>;
      };

   public:
      chunk_view() = default;
      chunk_view(V v, std::ranges::range_difference_t<V> n) : base_(std::move(v)), chunk_size_(n) {}

      constexpr auto begin() requires (!simple_view<V>) {
         return basic_iterator{ cursor<false>{ std::ranges::begin(base_), std::addressof(base_), chunk_size_ } };
      }

      constexpr auto begin() const requires (std::ranges::range<const V>) {
         return basic_iterator{ cursor<true>{ std::ranges::begin(base_), std::addressof(base_), chunk_size_ } };
      }

      constexpr auto end() requires (!simple_view<V>&& am_common<V>) {
         return basic_iterator{ cursor<false>(std::ranges::end(base_), std::addressof(base_), chunk_size_) };
      }

      constexpr auto end() const requires (std::ranges::range<const V>&& am_common<const V>) {
         return basic_iterator{ cursor<true>(std::ranges::end(base_), std::addressof(base_), chunk_size_) };
      }

      constexpr auto end() requires (!simple_view<V> && !am_common<V>) {
         return basic_sentinel<V, false>(std::ranges::end(base_));
      }

      constexpr auto end() const requires (std::ranges::range<const V> && !am_common<const V>) {
         return basic_sentinel<V, true>(std::ranges::end(base_));
      }

      constexpr auto size() requires (am_sized<V>) {
         return (std::ranges::size(base_) + chunk_size_ - 1) / chunk_size_;
      }

      constexpr auto size() const requires (am_sized<const V>) {
         return (std::ranges::size(base_) + chunk_size_ - 1) / chunk_size_;
      }

      auto& base() {
         return base_;
      }

      auto const& base() const {
         return base_;
      }
   };

   template <class R, class N>
   chunk_view(R&&, N n)->chunk_view<std::views::all_t<R>>;

   namespace views {
      namespace detail {
         struct chunk_fn_base {
            template <std::ranges::viewable_range R>
            constexpr auto operator()(R&& r, std::ranges::range_difference_t<R> n) const 
            requires std::ranges::forward_range<R> {
               return chunk_view( std::forward<R>(r),  n );
            }
         };

         struct chunk_fn : chunk_fn_base {
            using chunk_fn_base::operator();

            template <std::integral N>
            constexpr auto operator()(N n) const {
               return pipeable(bind_back(chunk_fn_base{}, n));
            }
         };
      }

      constexpr inline detail::chunk_fn chunk;
   }
}

namespace std::ranges {
   template <class R>
   inline constexpr bool enable_borrowed_range<tl::chunk_view<R>> = enable_borrowed_range<R>;
}

#endif