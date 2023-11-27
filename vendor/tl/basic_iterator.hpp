#ifndef TL_RANGES_BASIC_ITERATOR
#define TL_RANGES_BASIC_ITERATOR

#include <concepts>
#include <iterator>
#include <utility>
#include "common.hpp"

namespace tl {
   template <std::destructible T>
   class basic_mixin : protected T {
   public:
      constexpr basic_mixin()
         noexcept(std::is_nothrow_default_constructible<T>::value)
         requires std::default_initializable<T> :
         T() {}
      constexpr basic_mixin(const T& t)
         noexcept(std::is_nothrow_copy_constructible<T>::value)
         requires std::copy_constructible<T> :
         T(t) {}
      constexpr basic_mixin(T&& t)
         noexcept(std::is_nothrow_move_constructible<T>::value)
         requires std::move_constructible<T> :
         T(std::move(t)) {}


      constexpr T& get() & noexcept { return *static_cast<T*>(this); }
      constexpr const T& get() const& noexcept { return *static_cast<T const*>(this); }
      constexpr T&& get() && noexcept { return std::move(*static_cast<T*>(this)); }
      constexpr const T&& get() const&& noexcept { return std::move(*static_cast<T const*>(this)); }
   };

   namespace cursor {
      namespace detail {
         template <class C>
         struct tags {
            static constexpr auto single_pass() requires requires { { C::single_pass } -> std::convertible_to<bool>; } {
               return C::single_pass;
            }
            static constexpr auto single_pass() { return false; }

            static constexpr auto contiguous() requires requires { { C::contiguous } -> std::convertible_to<bool>; } {
               return C::contiguous;
            }
            static constexpr auto contiguous() { return false; }
         };
      }
      template <class C>
      constexpr bool single_pass = detail::tags<C>::single_pass();

      template <class C>
      constexpr bool tagged_contiguous = detail::tags<C>::contiguous();

      namespace detail {
         template <class C>
         struct deduced_mixin_t {
            template <class T> static auto deduce(int)->T::mixin;
            template <class T> static auto deduce(...)->tl::basic_mixin<T>;
            using type = decltype(deduce<C>(0));
         };
      }

      template <class C>
      using mixin_t = typename detail::deduced_mixin_t<C>::type;

      template <class C>
      requires
         requires(const C& c) { c.read(); }
      using reference_t = decltype(std::declval<const C&>().read());

      namespace detail {
         template <class C>
         struct deduced_value_t {
            template<class T> static auto deduce(int)->T::value_type;
            template<class T> static auto deduce(...)->std::decay_t<reference_t<T>>;

            using type = decltype(deduce<C>(0));
         };
      }

      template <class C>
      requires std::same_as<typename detail::deduced_value_t<C>::type, std::decay_t<typename detail::deduced_value_t<C>::type>>
         using value_type_t = typename detail::deduced_value_t<C>::type;

      namespace detail {
         template <class C>
         struct deduced_difference_t {
            template <class T> static auto deduce(int)->T::difference_type;
            template <class T>
            static auto deduce(long)->decltype(std::declval<const T&>().distance_to(std::declval<const T&>()));
            template <class T>
            static auto deduce(...)->std::ptrdiff_t;

            using type = decltype(deduce<C>(0));
         };
      }

      template <class C>
      using difference_type_t = typename detail::deduced_difference_t<C>::type;

      template <class C>
      concept cursor = std::semiregular<std::remove_cv_t<C>>
         && std::semiregular<mixin_t<std::remove_cv_t<C>>>
         && requires {typename difference_type_t<C>; };

      template <class C>
      concept readable = cursor<C> && requires(const C & c) {
         c.read();
         typename reference_t<C>;
         typename value_type_t<C>;
      };

      template <class C>
      concept arrow = readable<C>
         && requires(const C & c) { c.arrow(); };

      template <class C, class T>
      concept writable = cursor<C>
         && requires(C & c, T && t) { c.write(std::forward<T>(t)); };

      template <class S, class C>
      concept sentinel_for = cursor<C> && std::semiregular<S>
         && requires(const C & c, const S & s) { {c.equal(s)} -> std::same_as<bool>; };

      template <class S, class C>
      concept sized_sentinel_for = sentinel_for<S, C> &&
         requires(const C & c, const S & s) {
            {c.distance_to(s)} -> std::same_as<difference_type_t<C>>;
      };

      template <class C>
      concept next = cursor<C> && requires(C & c) { c.next(); };

      template <class C>
      concept prev = cursor<C> && requires(C & c) { c.prev(); };

      template <class C>
      concept advance = cursor<C>
         && requires(C & c, difference_type_t<C> n) { c.advance(n); };

      template <class C>
      concept indirect_move = readable<C>
         && requires(const C & c) { c.indirect_move(); };

      template <class C, class O>
      concept indirect_swap = readable<C> && readable<O>
         && requires(const C & c, const O & o) {
         c.indirect_swap(o);
         o.indirect_swap(c);
      };

      template <class C>
      concept input = readable<C> && next<C>;
      template <class C>
      concept forward = input<C> && sentinel_for<C, C> && !single_pass<C>;
      template <class C>
      concept bidirectional = forward<C> && prev<C>;
      template <class C>
      concept random_access = bidirectional<C> && advance<C> && sized_sentinel_for<C, C>;
      template <class C>
      concept contiguous = random_access<C> && tagged_contiguous<C> && std::is_reference_v<reference_t<C>>;

      template <class C>
      constexpr auto cpp20_iterator_category() {
         if constexpr (contiguous<C>)
            return std::contiguous_iterator_tag{};
         else if constexpr (random_access<C>)
            return std::random_access_iterator_tag{};
         else if constexpr (bidirectional<C>)
            return std::bidirectional_iterator_tag{};
         else if constexpr (forward<C>)
            return std::forward_iterator_tag{};
         else
            return std::input_iterator_tag{};
      }
      template <class C>
      using cpp20_iterator_category_t = decltype(cpp20_iterator_category<C>());

      //There were a few changes in requirements on iterators between C++17 and C++20
      //See https://wg21.link/p2259 for discussion
      //- C++17 didn't have contiguous iterators
      //- C++17 input iterators required *it++ to be valid
      //- C++17 forward iterators required the reference type to be exactly value_type&/value_type const& (i.e. not a proxy)
      struct not_a_cpp17_iterator {};

      template <class C>
      concept reference_is_value_type_ref =
         (std::same_as<reference_t<C>, value_type_t<C>&> || std::same_as<reference_t<C>, value_type_t<C> const&>);

      template <class C>
      concept can_create_postincrement_proxy =
         (std::move_constructible<value_type_t<C>> && std::constructible_from<value_type_t<C>, reference_t<C>>);

      template <class C>
      constexpr auto cpp17_iterator_category() {
         if constexpr (random_access<C> && reference_is_value_type_ref<C>)
            return std::random_access_iterator_tag{};
         else if constexpr (bidirectional<C> && reference_is_value_type_ref<C>)
            return std::bidirectional_iterator_tag{};
         else if constexpr (forward<C> && reference_is_value_type_ref<C>)
            return std::forward_iterator_tag{};
         else if constexpr (can_create_postincrement_proxy<C>)
            return std::input_iterator_tag{};
         else
            return not_a_cpp17_iterator{};
      }
      template <class C>
      using cpp17_iterator_category_t = decltype(cpp17_iterator_category<C>());

      //iterator_concept and iterator_category are tricky; this abstracts them out.
      //Since the rules for iterator categories changed between C++17 and C++20
      //a C++20 iterator may have a weaker category in C++17,
      //or it might not be a valid C++17 iterator at all.
      //iterator_concept will be the C++20 iterator category.
      //iterator_category will be the C++17 iterator category, or it will not exist
      //in the case that the iterator is not a valid C++17 iterator.
      template <cursor C, class category = cpp17_iterator_category_t<C>>
      struct associated_types_category_base {
         using iterator_category = category;
      };
      template <cursor C>
      struct associated_types_category_base<C, not_a_cpp17_iterator> {};

      template <cursor C>
      struct associated_types : associated_types_category_base<C> {
         using iterator_concept = cpp20_iterator_category_t<C>;
         using value_type = cursor::value_type_t<C>;
         using difference_type = cursor::difference_type_t<C>;
         using reference = cursor::reference_t<C>;
      };

      namespace detail {
         // We assume a cursor is writeable if it's either not readable
         // or it is writeable with the same type it reads to
         template <class C>
         struct is_writable_cursor {
            template <readable T>
            requires requires (C c) {
               c.write(c.read());
            }
            static auto deduce()->std::true_type;

            template <readable T>
            static auto deduce()->std::false_type;

            template <class T>
            static auto deduce()->std::true_type;

            static constexpr bool value = decltype(deduce<C>())::value;
         };
      }
   }

   namespace detail {
      template <class T>
      struct post_increment_proxy {
      private:
         T cache_;

      public:
         template<typename U>
         constexpr post_increment_proxy(U&& t)
            : cache_(std::forward<U>(t))
         {}
         constexpr T const& operator*() const noexcept
         {
            return cache_;
         }
      };
   }


   template <cursor::input C>
   class basic_iterator :
      public cursor::mixin_t<C>
   {
   private:
      using mixin = cursor::mixin_t<C>;

      constexpr auto& cursor() noexcept { return this->mixin::get(); }
      constexpr auto const& cursor() const noexcept { return this->mixin::get(); }

      template <cursor::input>
      friend class basic_iterator;

      //TODO these need to change to support output iterators
      using reference_t = decltype(std::declval<C>().read());
      using const_reference_t = reference_t;

   public:
      using mixin::get;

      using value_type = cursor::value_type_t<C>;
      using difference_type = cursor::difference_type_t<C>;
      using reference = cursor::reference_t<C>;

      basic_iterator() = default;

      using mixin::mixin;

      constexpr explicit basic_iterator(C&& c)
         noexcept(std::is_nothrow_constructible_v<mixin, C&&>) :
         mixin(std::move(c)) {}


      constexpr explicit basic_iterator(C const& c)
         noexcept(std::is_nothrow_constructible_v<mixin, C const&>) :
         mixin(c) {}

      template <std::convertible_to<C> O>
      constexpr basic_iterator(basic_iterator<O>&& that)
         noexcept(std::is_nothrow_constructible<mixin, O&&>::value) :
         mixin(that.cursor()) {}

      template <std::convertible_to<C> O>
      constexpr basic_iterator(const basic_iterator<O>& that)
         noexcept(std::is_nothrow_constructible<mixin, const O&>::value) :
         mixin(std::move(that.cursor())) {}

      template <std::convertible_to<C> O>
      constexpr basic_iterator& operator=(basic_iterator<O>&& that) &
         noexcept(std::is_nothrow_assignable<C&, O&&>::value) {
         cursor() = std::move(that.cursor());
         return *this;
      }

      template <std::convertible_to<C> O>
      constexpr basic_iterator& operator=(const basic_iterator<O>& that) &
         noexcept(std::is_nothrow_assignable<C&, const O&>::value) {
         cursor() = that.cursor();
         return *this;
      }

      template <class T>
      requires
         (!std::same_as<std::decay_t<T>, basic_iterator> &&
            !cursor::next<C>&&
            cursor::writable<C, T>)
         constexpr basic_iterator& operator=(T&& t) &
         noexcept(noexcept(std::declval<C&>().write(static_cast<T&&>(t)))) {
         cursor() = std::forward<T>(t);
         return *this;
      }

      friend constexpr decltype(auto) iter_move(const basic_iterator& i)
         noexcept(noexcept(i.cursor().indirect_move()))
         requires cursor::indirect_move<C> {
         return i.cursor().indirect_move();
      }

      template <class O>
      requires cursor::indirect_swap<C, O>
         friend constexpr void iter_swap(
            const basic_iterator& x, const basic_iterator<O>& y)
         noexcept(noexcept((void)x.indirect_swap(y))) {
         x.indirect_swap(y);
      }

      //Input iterator
      constexpr decltype(auto) operator*() const
         noexcept(noexcept(std::declval<const C&>().read()))
         requires (cursor::readable<C> && !cursor::detail::is_writable_cursor<C>::value) {
         return cursor().read();
      }

      //Output iterator
      constexpr decltype(auto) operator*()
         noexcept(noexcept(reference_t{ cursor() }))
         requires (cursor::next<C>&& cursor::detail::is_writable_cursor<C>::value) {
         return reference_t{ cursor() };
      }

      //Output iterator
      constexpr decltype(auto) operator*() const
         noexcept(noexcept(
            const_reference_t{ cursor() }))
         requires (cursor::next<C>&& cursor::detail::is_writable_cursor<C>::value) {
         return const_reference_t{ cursor() };
      }

      constexpr basic_iterator& operator*() noexcept
         requires (!cursor::next<C>) {
         return *this;
      }

      // operator->: "Manual" deduction override,
      constexpr decltype(auto) operator->() const
         noexcept(noexcept(cursor().arrow()))
         requires cursor::arrow<C> {
         return cursor().arrow();
      }
      // operator->: Otherwise, if reference_t is an lvalue reference,
      constexpr decltype(auto) operator->() const
         noexcept(noexcept(*std::declval<const basic_iterator&>()))
         requires (cursor::readable<C> && !cursor::arrow<C>)
         && std::is_lvalue_reference<const_reference_t>::value{
         return std::addressof(**this);
      }

         // modifiers
         constexpr basic_iterator& operator++() & noexcept {
         return *this;
      }
      constexpr basic_iterator& operator++() &
         noexcept(noexcept(cursor().next()))
         requires cursor::next<C> {
         cursor().next();
         return *this;
      }

      //C++17 required that *it++ was valid.
      //For input iterators, we can't copy *this, so we need to create a proxy reference.
      constexpr auto operator++(int) &
         noexcept(noexcept(++std::declval<basic_iterator&>()) &&
            std::is_nothrow_move_constructible_v<value_type>&&
            std::is_nothrow_constructible_v<value_type, reference>)
         requires (cursor::single_pass<C>&&
            std::move_constructible<value_type>&&
            std::constructible_from<value_type, reference>) {
         detail::post_increment_proxy<value_type> p(**this);
         ++* this;
         return p;
      }

      //If we can't create a proxy reference, it++ is going to return void
      constexpr void operator++(int) &
         noexcept(noexcept(++std::declval<basic_iterator&>()))
         requires (cursor::single_pass<C> && !(std::move_constructible<value_type>&&
            std::constructible_from<value_type, reference>)) {
         (void)(++(*this));
      }

      //If C is a forward cursor then copying it is fine
      constexpr basic_iterator operator++(int) &
         noexcept(std::is_nothrow_copy_constructible_v<C>&&
            std::is_nothrow_move_constructible_v<C> &&
            noexcept(++std::declval<basic_iterator&>()))
         requires (!cursor::single_pass<C>) {
         auto temp = *this;
         ++* this;
         return temp;
      }

      constexpr basic_iterator& operator--() &
         noexcept(noexcept(cursor().prev()))
         requires cursor::bidirectional<C> {
         cursor().prev();
         return *this;
      }

      //Postfix decrement doesn't have the same issue as postfix increment
      //because bidirectional requires the cursor to be a forward cursor anyway
      //so copying it is fine.
      constexpr basic_iterator operator--(int) &
         noexcept(std::is_nothrow_copy_constructible<basic_iterator>::value&&
            std::is_nothrow_move_constructible<basic_iterator>::value &&
            noexcept(--std::declval<basic_iterator&>()))
         requires cursor::bidirectional<C> {
         auto tmp = *this;
         --* this;
         return tmp;
      }

      constexpr basic_iterator& operator+=(difference_type n) &
         noexcept(noexcept(cursor().advance(n)))
         requires cursor::random_access<C> {
         cursor().advance(n);
         return *this;
      }

      constexpr basic_iterator& operator-=(difference_type n) &
         noexcept(noexcept(cursor().advance(-n)))
         requires cursor::random_access<C> {
         cursor().advance(-n);
         return *this;
      }

      constexpr decltype(auto) operator[](difference_type n) const
         noexcept(noexcept(*(std::declval<basic_iterator&>() + n)))
         requires cursor::random_access<C> {
         return *(*this + n);
      }

      // non-template type-symmetric ops to enable implicit conversions
      friend constexpr difference_type operator-(
         const basic_iterator& x, const basic_iterator& y)
         noexcept(noexcept(y.cursor().distance_to(x.cursor())))
         requires cursor::sized_sentinel_for<C, C> {
         return y.cursor().distance_to(x.cursor());
      }
      friend constexpr bool operator==(
         const basic_iterator& x, const basic_iterator& y)
         noexcept(noexcept(x.cursor().equal(y.cursor())))
         requires cursor::sentinel_for<C, C> {
         return x.cursor().equal(y.cursor());
      }
      friend constexpr bool operator!=(
         const basic_iterator& x, const basic_iterator& y)
         noexcept(noexcept(!(x == y)))
         requires cursor::sentinel_for<C, C> {
         return !(x == y);
      }
      friend constexpr bool operator<(
         const basic_iterator& x, const basic_iterator& y)
         noexcept(noexcept(y - x))
         requires cursor::sized_sentinel_for<C, C> {
         return 0 < (y - x);
      }
      friend constexpr bool operator>(
         const basic_iterator& x, const basic_iterator& y)
         noexcept(noexcept(y - x))
         requires cursor::sized_sentinel_for<C, C> {
         return 0 > (y - x);
      }
      friend constexpr bool operator<=(
         const basic_iterator& x, const basic_iterator& y)
         noexcept(noexcept(y - x))
         requires cursor::sized_sentinel_for<C, C> {
         return 0 <= (y - x);
      }
      friend constexpr bool operator>=(
         const basic_iterator& x, const basic_iterator& y)
         noexcept(noexcept(y - x))
         requires cursor::sized_sentinel_for<C, C> {
         return 0 >= (y - x);
      }
   };

   namespace detail {
      template <class C>
      struct is_basic_iterator {
         template <class T>
         static auto deduce(basic_iterator<T> const&)->std::true_type;
         template <class T>
         static auto deduce(...)->std::false_type;
         static constexpr inline bool value = decltype(deduce(std::declval<C>()))::value;
      };
   }

   // basic_iterator nonmember functions
   template <class C>
   constexpr basic_iterator<C> operator+(
      const basic_iterator<C>& i, cursor::difference_type_t<C> n)
      noexcept(std::is_nothrow_copy_constructible<basic_iterator<C>>::value&&
         std::is_nothrow_move_constructible<basic_iterator<C>>::value &&
         noexcept(std::declval<basic_iterator<C>&>() += n))
      requires cursor::random_access<C> {
      auto tmp = i;
      tmp += n;
      return tmp;
   }
   template <class C>
   constexpr basic_iterator<C> operator+(
      cursor::difference_type_t<C> n, const basic_iterator<C>& i)
      noexcept(noexcept(i + n))
      requires cursor::random_access<C> {
      return i + n;
   }

   template <class C>
   constexpr basic_iterator<C> operator-(
      const basic_iterator<C>& i, cursor::difference_type_t<C> n)
      noexcept(noexcept(i + (-n)))
      requires cursor::random_access<C> {
      return i + (-n);
   }
   template <class C1, class C2>
   requires cursor::sized_sentinel_for<C1, C2>
      constexpr cursor::difference_type_t<C2> operator-(
         const basic_iterator<C1>& lhs, const basic_iterator<C2>& rhs)
      noexcept(noexcept(
         rhs.get().distance_to(lhs.get()))) {
      return rhs.get().distance_to(lhs.get());
   }
   template <class C, class S>
   requires cursor::sized_sentinel_for<S, C>
      constexpr cursor::difference_type_t<C> operator-(
         const S& lhs, const basic_iterator<C>& rhs)
      noexcept(noexcept(rhs.get().distance_to(lhs))) {
      return rhs.get().distance_to(lhs);
   }
   template <class C, class S>
   requires cursor::sized_sentinel_for<S, C>
      constexpr cursor::difference_type_t<C> operator-(
         const basic_iterator<C>& lhs, const S& rhs)
      noexcept(noexcept(-(rhs - lhs))) {
      return -(rhs - lhs);
   }

   template <class C1, class C2>
   requires cursor::sentinel_for<C2, C1>
      constexpr bool operator==(
         const basic_iterator<C1>& lhs, const basic_iterator<C2>& rhs)
      noexcept(noexcept(lhs.get().equal(rhs.get()))) {
      return lhs.get().equal(rhs.get());
   }
   template <class C, class S>
   requires cursor::sentinel_for<S, C>
      constexpr bool operator==(
         const basic_iterator<C>& lhs, const S& rhs)
      noexcept(noexcept(lhs.get().equal(rhs))) {
      return lhs.get().equal(rhs);
   }
   template <class C, class S>
   requires cursor::sentinel_for<S, C>
      constexpr bool operator==(
         const S& lhs, const basic_iterator<C>& rhs)
      noexcept(noexcept(rhs == lhs)) {
      return rhs == lhs;
   }

   template <class C1, class C2>
   requires cursor::sentinel_for<C2, C1>
      constexpr bool operator!=(
         const basic_iterator<C1>& lhs, const basic_iterator<C2>& rhs)
      noexcept(noexcept(!(lhs == rhs))) {
      return !(lhs == rhs);
   }
   template <class C, class S>
   requires cursor::sentinel_for<S, C>
      constexpr bool operator!=(
         const basic_iterator<C>& lhs, const S& rhs)
      noexcept(noexcept(!lhs.get().equal(rhs))) {
      return !lhs.get().equal(rhs);
   }
   template <class C, class S>
   requires cursor::sentinel_for<S, C>
      constexpr bool operator!=(
         const S& lhs, const basic_iterator<C>& rhs)
      noexcept(noexcept(!rhs.get().equal(lhs))) {
      return !rhs.get().equal(lhs);
   }

   template <class C1, class C2>
   requires cursor::sized_sentinel_for<C1, C2>
      constexpr bool operator<(
         const basic_iterator<C1>& lhs, const basic_iterator<C2>& rhs)
      noexcept(noexcept(lhs - rhs < 0)) {
      return (lhs - rhs) < 0;
   }

   template <class C1, class C2>
   requires cursor::sized_sentinel_for<C1, C2>
      constexpr bool operator>(
         const basic_iterator<C1>& lhs, const basic_iterator<C2>& rhs)
      noexcept(noexcept((lhs - rhs) > 0)) {
      return (lhs - rhs) > 0;
   }

   template <class C1, class C2>
   requires cursor::sized_sentinel_for<C1, C2>
      constexpr bool operator<=(
         const basic_iterator<C1>& lhs, const basic_iterator<C2>& rhs)
      noexcept(noexcept((lhs - rhs) <= 0)) {
      return (lhs - rhs) <= 0;
   }

   template <class C1, class C2>
   requires cursor::sized_sentinel_for<C1, C2>
      constexpr bool operator>=(
         const basic_iterator<C1>& lhs, const basic_iterator<C2>& rhs)
      noexcept(noexcept((lhs - rhs) >= 0)) {
      return (lhs - rhs) >= 0;
   }

   template <class V, bool Const>
   class basic_sentinel {
      using Base = std::conditional_t<Const, const V, V>;

   public:
      std::ranges::sentinel_t<Base> end_{};
      basic_sentinel() = default;
      constexpr explicit basic_sentinel(std::ranges::sentinel_t<Base> end)
         : end_{ std::move(end) } {}

      constexpr basic_sentinel(basic_sentinel<V, !Const> other) requires Const&& std::
         convertible_to<std::ranges::sentinel_t<V>,
         std::ranges::sentinel_t<Base>>
         : end_{ std::move(other.end_) } {}

      constexpr auto end() const {
         return end_;
      }

      friend class basic_sentinel<V, !Const>;
   };
}

namespace std {
   template <class C>
   struct iterator_traits<tl::basic_iterator<C>> : tl::cursor::associated_types<C> {};
}
#endif