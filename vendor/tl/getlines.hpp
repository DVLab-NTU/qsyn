#ifndef TL_RANGES_GETLINES_N_HPP
#define TL_RANGES_GETLINES_N_HPP

#include "common.hpp"
#include "basic_iterator.hpp"
#include <ranges>
#include <string>
#include <iostream>
#include "functional/pipeable.hpp"
#include "functional/bind.hpp"

namespace tl {
	class getlines_view : public std::ranges::view_interface<getlines_view> {
		std::istream* in_;
		std::string str_;
		char delim_;

		struct cursor {
		private:
			getlines_view* parent_;

		public:
			static constexpr bool single_pass = true;
			cursor() = default;

			explicit cursor(getlines_view* parent) :
				parent_{ parent } {}

			void next() {
				parent_->next();
			}

			std::string& read() const noexcept {
				return parent_->str_;
			}

			bool equal(std::default_sentinel_t) const {
				return !parent_->in_;
			}
			bool equal(cursor const& rhs) const {
				return !parent_->in_ == !rhs.parent_->in_;
			}
		};

		void next() {
			if (!std::getline(*in_, str_, delim_)) {
				in_ = nullptr;
			}
		}

	public:
		getlines_view() = default;
		getlines_view(std::istream& in, char delim = '\n')
			: in_(std::addressof(in)), str_(), delim_(delim)
		{
			next();
		}

		auto begin() {
			return tl::basic_iterator{ cursor{this} };
		}
		auto end() const noexcept {
			return std::default_sentinel;
		}
	};

	namespace views {
		namespace detail {
			struct getlines_fn {
				getlines_view operator()(std::istream& in, char delim = '\n') const
				{
					return getlines_view{ in, delim };
				}
			};
		}

		inline constexpr auto getlines = detail::getlines_fn{};
	}
}
#endif