#ifndef TL_RANGES_SLIDE_HPP
#define TL_RANGES_SLIDE_HPP

#include <ranges>
#include <iterator>
#include "common.hpp"
#include "basic_iterator.hpp"
#include "utility/non_propagating_cache.hpp"
#include "functional/bind.hpp"
#include "functional/pipeable.hpp"

namespace tl {
    // Slide view may need to cache either the begin or end iterator
    namespace detail {
        template <class V>
        concept slide_caches_nothing = std::ranges::random_access_range<V> && std::ranges::sized_range<V>;
        template <class V>
        concept slide_caches_last = !slide_caches_nothing<V> && std::ranges::bidirectional_range<V> && std::ranges::common_range<V>;
        template <class V>
        concept slide_caches_first = !slide_caches_nothing<V> && !slide_caches_last<V>;
    }

    // No cache
    template <class V, bool = detail::slide_caches_last<V>, bool = detail::slide_caches_first<V>>
    struct slide_view_base {};

    // Cache last
    template <class V>
    struct slide_view_base<V, true, false> {
        tl::non_propagating_cache<std::ranges::iterator_t<V>> last_cache_;
    };

    // Cache first
    template <class V>
    struct slide_view_base<V, false, true> {
        tl::non_propagating_cache<std::ranges::iterator_t<V>> first_cache_;

    };

    template <std::ranges::forward_range V>
        requires std::ranges::view<V> class slide_view
    : public slide_view_base<V>, public std::ranges::view_interface<slide_view<V>> {
        private:
            V base_ = V();
            std::ranges::range_difference_t<V> n_ = 0;

            template <bool Const, class Base = std::conditional_t<Const, const V, V>, bool = detail::slide_caches_first<Base>>
            class cursor_base {
            protected:
                std::ranges::iterator_t<Base> current_ = std::ranges::iterator_t<Base>();
                std::ranges::range_difference_t<Base> n_ = 0;

                constexpr cursor_base() = default;
                constexpr cursor_base(std::ranges::iterator_t<Base> current, std::ranges::range_difference_t<Base> n)
                    : current_(std::move(current)), n_(n) {}
            };

            template <bool Const, class Base>
            class cursor_base<Const, Base, true> {
            protected:
                std::ranges::iterator_t<Base> current_ = std::ranges::iterator_t<Base>();
                std::ranges::iterator_t<Base> last_ele_ = std::ranges::iterator_t<Base>();
                std::ranges::range_difference_t<Base> n_ = 0;

                constexpr cursor_base() = default;
                constexpr cursor_base(std::ranges::iterator_t<Base> current, std::ranges::iterator_t<Base> last_ele, std::ranges::range_difference_t<Base> n)
                    : current_(std::move(current)), last_ele_(std::move(last_ele)), n_(n) {}
            };

            template <bool Const>
            struct cursor : public cursor_base<Const> {
                template <class T>
                using constify = std::conditional_t<Const, const T, T>;
                using Base = constify<V>;
                using difference_type = std::ranges::range_difference_t<Base>;

                constexpr cursor() = default;

                constexpr cursor(cursor<!Const> i)
                    requires Const&& std::convertible_to<std::ranges::iterator_t<V>, std::ranges::iterator_t<Base>>
                : cursor_base<Const>(std::move(i.current_), (i.n_)) {}


                constexpr cursor(std::ranges::iterator_t<Base> current, std::ranges::range_difference_t<Base> n)
                    requires (!detail::slide_caches_first<Base>)
                : cursor_base<Const>(std::move(current), n) {}

                constexpr cursor(std::ranges::iterator_t<Base> current, std::ranges::iterator_t<Base> last_ele, std::ranges::range_difference_t<Base> n)
                    requires detail::slide_caches_first<Base>
                : cursor_base<Const>(std::move(current), last_ele, n) {}

                constexpr auto read() const {
                    return std::views::counted(this->current_, this->n_);
                }

                constexpr void next() {
                    std::ranges::advance(this->current_, 1);
                    if constexpr (detail::slide_caches_first<Base>) {
                        std::ranges::advance(this->last_ele_, 1);
                    }
                }

                constexpr void prev() requires std::ranges::bidirectional_range<Base> {
                    std::ranges::advance(this->current_, -1);
                    if constexpr (detail::slide_caches_first<Base>) {
                        std::ranges::advance(this->last_ele_, -1);
                    }
                }

                constexpr void advance(difference_type x) requires std::ranges::random_access_range<Base> {
                    this->current_ += x;
                    if constexpr (detail::slide_caches_first<Base>) {
                        this->last_ele_ += x;
                    }
                }

                constexpr bool equal(cursor const& rhs) const {
                    if constexpr (detail::slide_caches_first<Base>) {
                        return this->last_ele_ == rhs.last_ele_;
                    }
                    else {
                        return this->current_ == rhs.current_;
                    }
                }

                constexpr bool equal(basic_sentinel<V, false> const& rhs) const
                    requires detail::slide_caches_first<Base> {
                    return this->last_ele_ == rhs.end_;
                }

                constexpr auto distance_to(const cursor& rhs) const
                requires std::ranges::random_access_range<Base>{
                    if constexpr (detail::slide_caches_first<Base>) {
                        return rhs.last_ele_ - this->last_ele_;
                    }
                    else {
                        return rhs.current_ - this->current_;
                    }
                }

                friend struct cursor<!Const>;
            };

        public:
            slide_view() requires std::default_initializable<V> = default;
            constexpr explicit slide_view(V base, std::ranges::range_difference_t<V> n)
                : base_(std::move(base)), n_(n) {}

            constexpr auto begin()
                requires (!(simple_view<V>&& detail::slide_caches_nothing<const V>)) {
                if constexpr (detail::slide_caches_first<V>) {
                    if (!this->first_cache_) {
                        this->first_cache_.emplace(std::ranges::next(std::ranges::begin(base_), n_ - 1, std::ranges::end(base_)));
                    }
                    return basic_iterator{ cursor<false>(std::ranges::begin(base_), *this->first_cache_, n_) };
                }
                else {
                    return basic_iterator(cursor<false>(std::ranges::begin(base_), n_));
                }
            }
            constexpr auto begin() const requires detail::slide_caches_nothing<const V> {
                return basic_iterator<cursor<true>>{ cursor<true>(std::ranges::begin(base_), n_) };
            }

            constexpr auto end()
                requires (!(simple_view<V>&& detail::slide_caches_nothing<const V>)) {
                if constexpr (detail::slide_caches_nothing<V>) {
                    return basic_iterator(cursor<false>(std::ranges::begin(base_) + std::ranges::range_difference_t<V>(size()), n_));
                }
                else if constexpr (detail::slide_caches_last<V>) {
                    if (!this->last_cache_) {
                        this->last_cache_.emplace(std::ranges::prev(std::ranges::end(base_), n_ - 1, std::ranges::begin(base_)));
                    }
                    return basic_iterator(cursor<false>(*this->last_cache_, n_));
                }
                else if constexpr (std::ranges::common_range<V>) {
                    return basic_iterator(cursor<false>(std::ranges::end(base_), std::ranges::end(base_), n_));
                }
                else {
                    return basic_sentinel<V, false>(std::ranges::end(base_));
                }
            }
            constexpr auto end() const requires detail::slide_caches_nothing<const V> {
                return begin() + std::ranges::range_difference_t<const V>(size());
            }

            constexpr auto size() requires std::ranges::sized_range<V> {
                auto sz = std::ranges::distance(base_) - n_ + 1;
                if (sz < 0) sz = 0;
                return sz;
            }

            constexpr auto size() const requires std::ranges::sized_range<const V> {
                auto sz = std::ranges::distance(base_) - n_ + 1;
                if (sz < 0) sz = 0;
                return sz;
            }
    };

    template<class R>
    slide_view(R&& r, std::ranges::range_difference_t<R>) -> slide_view<std::views::all_t<R>>;

    namespace views {
        namespace detail {
            struct slide_fn_base {
                template <std::ranges::viewable_range R>
                constexpr auto operator()(R&& r, std::ranges::range_difference_t<R> n) const
                    requires std::ranges::forward_range<R> {
                    return slide_view(std::forward<R>(r), n);
                }
            };

            struct slide_fn : slide_fn_base {
                using slide_fn_base::operator();

                template <std::integral N>
                constexpr auto operator()(N n) const {
                    return pipeable(bind_back(slide_fn_base{}, n));
                }
            };
        }

        constexpr inline detail::slide_fn slide;
    }
}

namespace std::ranges {
    template <class R>
    inline constexpr bool enable_borrowed_range<tl::slide_view<R>> = enable_borrowed_range<R>;
}
#endif