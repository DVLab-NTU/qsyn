/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Definition of the Phase class and pertinent classes ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cmath>
#include <iosfwd>
#include <numbers>
#include <string>
#include <vector>

#include "util/rational.hpp"
#include "util/util.hpp"

namespace dvlab {

class Rational;

template <typename T>
concept unitless = requires(T t) {
    std::is_arithmetic_v<T> == true || std::same_as<T, Rational> == true;
};

class Phase {
public:
    constexpr Phase() : _rational(0, 1) {}
    // explicitly ban `Phase phase = n;` to prevent confusing code
    constexpr explicit Phase(int n) : _rational(n, 1) { normalize(); }
    constexpr Phase(int n, int d) : _rational(n, d) { normalize(); }
    template <class T>
    requires std::floating_point<T>
    Phase(T f, T eps = 1e-4) : _rational(f / std::numbers::pi_v<T>, eps / std::numbers::pi_v<T>) { normalize(); }

    friend std::ostream& operator<<(std::ostream& os, Phase const& p);
    Phase operator+() const;
    Phase operator-() const;

    // Addition and subtraction are mod 2pi
    Phase& operator+=(Phase const& rhs);
    Phase& operator-=(Phase const& rhs);
    friend Phase operator+(Phase lhs, Phase const& rhs);
    friend Phase operator-(Phase lhs, Phase const& rhs);

    // Multiplication / Devision w/ unitless constants
    Phase& operator*=(const unitless auto& rhs);
    Phase& operator/=(const unitless auto& rhs);
    friend Phase operator*(Phase lhs, const unitless auto& rhs);
    friend Phase operator*(const unitless auto& lhs, Phase rhs);
    friend Phase operator/(Phase lhs, const unitless auto& rhs);
    friend Rational operator/(Phase const& lhs, Phase const& rhs);
    // Operator *, / between phases are not supported deliberately as they don't make physical sense (Changes unit)

    bool operator==(Phase const& rhs) const;
    bool operator!=(Phase const& rhs) const;
    // Operator <, <=, >, >= are are not supported deliberately as they don't make physical sense (Phases are mod 2pi)

    template <class T>
    requires std::floating_point<T>
    static T phase_to_floating_point(Phase const& p) { return std::numbers::pi_v<T> * Rational::rational_to_floating_point<T>(p._rational); }

    static float phase_to_f(Phase const& p) { return phase_to_floating_point<float>(p); }
    static double phase_to_d(Phase const& p) { return phase_to_floating_point<double>(p); }
    static long double phase_to_ld(Phase const& p) { return phase_to_floating_point<long double>(p); }

    Rational get_rational() const { return _rational; }
    int numerator() const { return _rational.numerator(); }
    int denominator() const { return _rational.denominator(); }

    template <class T>
    requires std::floating_point<T>
    static Phase to_phase(T f, T eps = 1e-4) {
        Phase p(f, eps);
        return p;
    }

    std::string get_ascii_string() const;
    std::string get_print_string() const;

    void normalize();

    template <class T = double>
    requires std::floating_point<T>
    static bool
    from_string(std::string const& str, Phase& phase) {
        if (!str_to_phase<T>(str, phase)) {
            phase = Phase(0);
            return false;
        }
        return true;
    }

    template <class T = double>
    requires std::floating_point<T>
    static bool str_to_phase(std::string const& str, Phase& p);

private:
    dvlab::Rational _rational;
};

Phase& Phase::operator*=(unitless auto const& rhs) {
    this->_rational *= rhs;
    normalize();
    return *this;
}
Phase& Phase::operator/=(unitless auto const& rhs) {
    this->_rational /= rhs;
    normalize();
    return *this;
}
Phase operator*(Phase lhs, unitless auto const& rhs) {
    lhs *= rhs;
    return lhs;
}
Phase operator*(unitless auto const& lhs, Phase rhs) {
    return rhs * lhs;
}
Phase operator/(Phase lhs, unitless auto const& rhs) {
    lhs /= rhs;
    return lhs;
}

template <class T>
requires std::floating_point<T>
bool Phase::str_to_phase(std::string const& str, Phase& p) {
    std::vector<std::string> number_strings;
    std::vector<char> operators;

    // string parsing
    size_t curr_pos = 0;
    size_t op_pos = 0;
    while (op_pos != std::string::npos) {
        op_pos = str.find_first_of("*/", curr_pos);
        if (op_pos != std::string::npos) {
            operators.emplace_back(str[op_pos]);
            number_strings.emplace_back(str.substr(curr_pos, op_pos - curr_pos));
        } else {
            number_strings.emplace_back(str.substr(curr_pos));
        }
        curr_pos = op_pos + 1;
    }

    // Error detection
    if (operators.size() >= number_strings.size()) return false;

    int n_pis = 0;
    int numerator = 1, denominator = 1;
    T temp_float = 1.0;

    int buffer_int;
    T buffer_float;

    bool do_division = false;

    for (size_t i = 0; i < number_strings.size(); ++i) {
        do_division = (i != 0 && operators[i - 1] == '/');

        if (dvlab::str::tolower_string(number_strings[i]) == "pi") {
            if (do_division)
                n_pis -= 1;
            else
                n_pis += 1;
        } else if (dvlab::str::tolower_string(number_strings[i]) == "-pi") {
            numerator *= -1;
            if (do_division)
                n_pis -= 1;
            else
                n_pis += 1;
        } else if (dvlab::str::str_to_i(number_strings[i], buffer_int)) {
            if (do_division)
                denominator *= buffer_int;
            else
                numerator *= buffer_int;
        } else if (dvlab::str::str_to_num<T>(number_strings[i], buffer_float)) {
            if (do_division)
                temp_float /= buffer_float;
            else
                temp_float *= buffer_float;
        } else {
            return false;
        }
    }

    dvlab::Rational tmp_rational(temp_float * std::pow(std::numbers::pi_v<T>, n_pis - 1), 1e-4 / std::numbers::pi_v<T>);

    p = Phase(numerator, denominator) * tmp_rational;

    return true;
}

}  // namespace dvlab

template <>
struct fmt::formatter<dvlab::Phase> {
    char presentation = 'e';  // Presentation format: 'f' - fixed, 'e' - scientific.

    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'f' || *it == 'e')) presentation = *it++;
        if (it != end && *it != '}') detail::throw_format_error("invalid format");
        return it;
    }
    auto format(dvlab::Phase const& p, format_context& ctx) const -> format_context::iterator {
        return presentation == 'f'
                   ? fmt::format_to(ctx.out(), "{}", dvlab::Phase::phase_to_d(p))
                   : fmt::format_to(ctx.out(), "{}", p.get_print_string());
    }
};
