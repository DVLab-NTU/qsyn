// SPDX-License-Identifier: MIT
#include "cppgridsynth/ring.hpp"

#include <sstream>
#include <stdexcept>

#include "cppgridsynth/mymath.hpp"

namespace cppgridsynth {

// ============================================================
//  ZRootTwo
// ============================================================

ZRootTwo ZRootTwo::from_zomega(const ZOmega& x) {
    if (x.b() == 0 && x.a() == -x.c()) {
        return ZRootTwo(x.d(), x.c());
    }
    throw std::invalid_argument("ZRootTwo::from_zomega: not in Z[√2]");
}

bool ZRootTwo::operator<(const ZRootTwo& o) const {
    // pygridsynth's comparison via decisional approach using a²-2b².
    if (b_ < o.b_) {
        bool first = a_ < o.a_;
        bool second = (a_ - o.a_) * (a_ - o.a_) < 2 * (b_ - o.b_) * (b_ - o.b_);
        return first || second;
    } else {
        bool first = a_ < o.a_;
        bool second = (a_ - o.a_) * (a_ - o.a_) > 2 * (b_ - o.b_) * (b_ - o.b_);
        return first && second;
    }
}

ZRootTwo ZRootTwo::pow(unsigned long n) const {
    ZRootTwo result(1, 0);
    ZRootTwo base = *this;
    while (n > 0) {
        if (n & 1) result *= base;
        base *= base;
        n >>= 1;
    }
    return result;
}

ZRootTwo ZRootTwo::pow(long n) const {
    if (n < 0) return inv().pow(static_cast<unsigned long>(-n));
    return pow(static_cast<unsigned long>(n));
}

ZRootTwo ZRootTwo::inv() const {
    mpz_class n = norm();
    if (n == 1)      return conj_sq2();
    if (n == -1)     return -conj_sq2();
    throw std::domain_error("ZRootTwo::inv: not a unit");
}

std::optional<ZRootTwo> ZRootTwo::sqrt() const {
    mpz_class n = norm();
    if (n < 0 || a_ < 0) return std::nullopt;
    mpz_class r = floorsqrt(n);
    mpz_class apr = (a_ + r) / 2;
    mpz_class amr = (a_ - r) / 2;
    mpz_class apr4 = (a_ + r) / 4;
    mpz_class amr4 = (a_ - r) / 4;
    if (apr < 0 || amr < 0 || amr4 < 0 || apr4 < 0) return std::nullopt;
    mpz_class a1 = floorsqrt(apr);
    mpz_class b1 = floorsqrt(amr4);
    mpz_class a2 = floorsqrt(amr);
    mpz_class b2 = floorsqrt(apr4);
    int sgnab = sign(a_) * sign(b_);
    ZRootTwo w1, w2;
    if (sgnab >= 0) { w1 = ZRootTwo(a1, b1); w2 = ZRootTwo(a2, b2); }
    else            { w1 = ZRootTwo(a1, -b1); w2 = ZRootTwo(a2, -b2); }
    if (*this == w1 * w1) return w1;
    if (*this == w2 * w2) return w2;
    return std::nullopt;
}

std::pair<ZRootTwo, ZRootTwo> ZRootTwo::divmod(const ZRootTwo& a, const ZRootTwo& b) {
    ZRootTwo p = a * b.conj_sq2();
    mpz_class k = b.norm();
    if (k == 0) throw std::domain_error("ZRootTwo::divmod: division by zero");
    ZRootTwo q(rounddiv(p.a_, k), rounddiv(p.b_, k));
    ZRootTwo r = a - b * q;
    return {q, r};
}

bool ZRootTwo::sim(const ZRootTwo& a, const ZRootTwo& b) {
    if (b == 0) return a == 0;
    if (a == 0) return false;
    return a.mod(b) == 0 && b.mod(a) == 0;
}

std::tuple<ZRootTwo, ZRootTwo, ZRootTwo> ZRootTwo::ext_gcd(ZRootTwo a, ZRootTwo b) {
    ZRootTwo x = ZRootTwo::from_int(1);
    ZRootTwo y = ZRootTwo::from_int(0);
    ZRootTwo z = ZRootTwo::from_int(0);
    ZRootTwo w = ZRootTwo::from_int(1);
    while (!(b == 0)) {
        auto [q, r] = divmod(a, b);
        ZRootTwo nx = x - y * q;
        ZRootTwo nz = z - w * q;
        x = y; y = nx;
        z = w; w = nz;
        a = b; b = r;
    }
    return {x, z, a};
}

ZRootTwo ZRootTwo::gcd(ZRootTwo a, ZRootTwo b) {
    auto [x, z, g] = ext_gcd(std::move(a), std::move(b));
    return g;
}

MPFloat ZRootTwo::to_real() const {
    return MPFloat(a_) + MPFloat::sqrt2() * MPFloat(b_);
}

std::string ZRootTwo::to_string() const {
    std::ostringstream os;
    os << a_ << (b_ >= 0 ? "+" : "") << b_ << "√2";
    return os.str();
}

// ============================================================
//  DRootTwo
// ============================================================
DRootTwo DRootTwo::from_zomega(const ZOmega& v) { return DRootTwo(ZRootTwo::from_zomega(v), 0); }
DRootTwo DRootTwo::from_domega(const DOmega& v) { return DRootTwo(ZRootTwo::from_zomega(v.u()), v.k()); }

bool DRootTwo::operator==(const DRootTwo& o) const {
    if (k_ < o.k_) return renew_denomexp(o.k_) == o;
    if (k_ > o.k_) return *this == o.renew_denomexp(k_);
    return alpha_ == o.alpha_;
}

bool DRootTwo::operator<(const DRootTwo& o) const {
    if (k_ < o.k_) return renew_denomexp(o.k_) < o;
    if (k_ > o.k_) return *this < o.renew_denomexp(k_);
    return alpha_ < o.alpha_;
}

DRootTwo operator+(const DRootTwo& l, const DRootTwo& r) {
    if (l.k_ < r.k_) return l.renew_denomexp(r.k_) + r;
    if (l.k_ > r.k_) return l + r.renew_denomexp(l.k_);
    return DRootTwo(l.alpha_ + r.alpha_, l.k_);
}
DRootTwo operator-(const DRootTwo& l, const DRootTwo& r) { return l + (-r); }

DRootTwo DRootTwo::renew_denomexp(long new_k) const {
    return DRootTwo(mul_by_sqrt2_power(new_k - k_).alpha_, new_k);
}

DRootTwo DRootTwo::reduce_denomexp() const {
    long k_a = (alpha_.a() == 0) ? k_ : ntz(alpha_.a());
    long k_b = (alpha_.b() == 0) ? k_ : ntz(alpha_.b());
    long new_k = (k_a <= k_b) ? (k_ - k_a * 2) : (k_ - k_b * 2 - 1);
    if (new_k < 0) new_k = 0;
    return renew_denomexp(new_k);
}

DRootTwo DRootTwo::mul_by_inv_sqrt2() const {
    if ((alpha_.a() & mpz_class(1)) != 0) {
        throw std::domain_error("DRootTwo::mul_by_inv_sqrt2: alpha.a not even");
    }
    ZRootTwo new_alpha(alpha_.b(), alpha_.a() / 2);
    return DRootTwo(new_alpha, k_);
}

DRootTwo DRootTwo::mul_by_sqrt2_power(long d) const {
    if (d == 0) return *this;
    if (d < 0) {
        if (d == -1) return mul_by_inv_sqrt2();
        long abs_d = -d;
        long div2 = abs_d >> 1;
        long mod2 = abs_d & 1;
        if (mod2 == 0) {
            mpz_class bit = (mpz_class(1) << static_cast<unsigned long>(div2)) - 1;
            if ((alpha_.a() & bit) != 0 || (alpha_.b() & bit) != 0) {
                throw std::domain_error("DRootTwo::mul_by_sqrt2_power: divisibility failed");
            }
            ZRootTwo new_alpha(alpha_.a() >> static_cast<mp_bitcnt_t>(div2),
                               alpha_.b() >> static_cast<mp_bitcnt_t>(div2));
            return DRootTwo(new_alpha, k_);
        } else {
            mpz_class bit = (mpz_class(1) << static_cast<unsigned long>(div2)) - 1;
            mpz_class bit2 = (mpz_class(1) << static_cast<unsigned long>(div2 + 1)) - 1;
            if ((alpha_.a() & bit2) != 0 || (alpha_.b() & bit) != 0) {
                throw std::domain_error("DRootTwo::mul_by_sqrt2_power: divisibility failed");
            }
            ZRootTwo new_alpha(alpha_.b() >> static_cast<mp_bitcnt_t>(div2),
                               alpha_.a() >> static_cast<mp_bitcnt_t>(div2 + 1));
            return DRootTwo(new_alpha, k_);
        }
    }
    long div2 = d >> 1;
    long mod2 = d & 1;
    mpz_class shift_amt = mpz_class(1) << static_cast<unsigned long>(div2);
    ZRootTwo new_alpha = alpha_ * ZRootTwo::from_int(shift_amt);
    if (mod2) new_alpha = new_alpha * ZRootTwo(0, 1);
    return DRootTwo(new_alpha, k_);
}

DRootTwo DRootTwo::mul_by_sqrt2_power_renewing_denomexp(long d) const {
    if (d > k_) throw std::domain_error("mul_by_sqrt2_power_renewing_denomexp: d>k");
    return DRootTwo(alpha_, k_ - d);
}

DRootTwo DRootTwo::conj_sq2() const {
    return (k_ & 1) ? DRootTwo(-alpha_.conj_sq2(), k_)
                    : DRootTwo(alpha_.conj_sq2(), k_);
}

MPFloat DRootTwo::scale() const { return pow_sqrt2(k_); }
MPFloat DRootTwo::to_real() const { return alpha_.to_real() / scale(); }

std::string DRootTwo::to_string() const {
    std::ostringstream os;
    os << alpha_.to_string() << " / √2^" << k_;
    return os.str();
}

// ============================================================
//  ZOmega
// ============================================================

ZOmega operator*(const ZOmega& l, const ZOmega& r) {
    auto cl = l.coef();   // [d, c, b, a]
    auto cr = r.coef();
    std::array<mpz_class, 4> nc{0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            mpz_class p = cl[i] * cr[j];
            if (i + j < 4) nc[i + j] += p;
            else           nc[i + j - 4] -= p;
        }
    }
    return ZOmega(nc[3], nc[2], nc[1], nc[0]);  // a, b, c, d ordering
}

ZOmega ZOmega::pow(unsigned long n) const {
    ZOmega out = ZOmega::from_int(1);
    ZOmega base = *this;
    while (n > 0) {
        if (n & 1) out *= base;
        base *= base;
        n >>= 1;
    }
    return out;
}

mpz_class ZOmega::norm() const {
    mpz_class part1 = a_ * a_ + b_ * b_ + c_ * c_ + d_ * d_;
    mpz_class part2 = a_ * b_ + b_ * c_ + c_ * d_ - d_ * a_;
    return part1 * part1 - 2 * part2 * part2;
}

ZOmega ZOmega::inv() const {
    mpz_class n = norm();
    if (n == 1) return conj_sq2() * conj() * conj().conj_sq2();
    throw std::domain_error("ZOmega::inv: not a unit");
}

std::pair<ZOmega, ZOmega> ZOmega::divmod(const ZOmega& a, const ZOmega& b) {
    ZOmega p = a * b.conj() * b.conj().conj_sq2() * b.conj_sq2();
    mpz_class k = b.norm();
    if (k == 0) throw std::domain_error("ZOmega::divmod: division by zero");
    ZOmega q(rounddiv(p.a_, k),
             rounddiv(p.b_, k),
             rounddiv(p.c_, k),
             rounddiv(p.d_, k));
    ZOmega r = a - b * q;
    return {q, r};
}

bool ZOmega::sim(const ZOmega& a, const ZOmega& b) {
    if (b == 0) return a == 0;
    if (a == 0) return false;
    return a.mod(b) == 0 && b.mod(a) == 0;
}

std::tuple<ZOmega, ZOmega, ZOmega> ZOmega::ext_gcd(ZOmega a, ZOmega b) {
    ZOmega x = ZOmega::from_int(1);
    ZOmega y = ZOmega::from_int(0);
    ZOmega z = ZOmega::from_int(0);
    ZOmega w = ZOmega::from_int(1);
    while (!(b == 0)) {
        auto [q, r] = divmod(a, b);
        ZOmega nx = x - y * q;
        ZOmega nz = z - w * q;
        x = y; y = nx;
        z = w; w = nz;
        a = b; b = r;
    }
    return {x, z, a};
}

ZOmega ZOmega::gcd(ZOmega a, ZOmega b) {
    auto [x, z, g] = ext_gcd(std::move(a), std::move(b));
    return g;
}

ZOmega ZOmega::mul_by_omega_power(long n) const {
    n = ((n % 8) + 8) % 8;
    if (n & 0b100) {
        return (-(*this)).mul_by_omega_power(n & 0b11);
    }
    auto cf = coef();             // [d, c, b, a]
    std::array<mpz_class, 4> nc{0, 0, 0, 0};
    for (long i = 0; i < n; ++i) {
        nc[i] = -cf[(i - n + 4) % 4];
    }
    for (long i = n; i < 4; ++i) {
        nc[i] = cf[i - n];
    }
    return ZOmega(nc[3], nc[2], nc[1], nc[0]);
}

int ZOmega::residue() const {
    int ra = (mpz_odd_p(a_.get_mpz_t()) ? 1 : 0);
    int rb = (mpz_odd_p(b_.get_mpz_t()) ? 1 : 0);
    int rc = (mpz_odd_p(c_.get_mpz_t()) ? 1 : 0);
    int rd = (mpz_odd_p(d_.get_mpz_t()) ? 1 : 0);
    return (ra << 3) | (rb << 2) | (rc << 1) | rd;
}

MPFloat ZOmega::real() const {
    // d + (c-a)*sqrt(2)/2
    MPFloat sq2 = MPFloat::sqrt2();
    return MPFloat(d_) + (sq2 * MPFloat(c_ - a_)) / MPFloat(2);
}
MPFloat ZOmega::imag() const {
    // b + (c+a)*sqrt(2)/2
    MPFloat sq2 = MPFloat::sqrt2();
    return MPFloat(b_) + (sq2 * MPFloat(c_ + a_)) / MPFloat(2);
}

std::string ZOmega::to_string() const {
    std::ostringstream os;
    os << a_ << "ω^3" << (b_ >= 0 ? "+" : "") << b_
       << "ω^2" << (c_ >= 0 ? "+" : "") << c_
       << "ω"   << (d_ >= 0 ? "+" : "") << d_;
    return os.str();
}

// ============================================================
//  DOmega
// ============================================================

bool DOmega::operator==(const DOmega& o) const {
    if (k_ < o.k_) return renew_denomexp(o.k_) == o;
    if (k_ > o.k_) return *this == o.renew_denomexp(k_);
    return u_ == o.u_;
}

DOmega operator+(const DOmega& l, const DOmega& r) {
    if (l.k_ < r.k_) return l.renew_denomexp(r.k_) + r;
    if (l.k_ > r.k_) return l + r.renew_denomexp(l.k_);
    return DOmega(l.u_ + r.u_, l.k_);
}
DOmega operator-(const DOmega& l, const DOmega& r) { return l + (-r); }

DOmega DOmega::from_droottwo_vector(const DRootTwo& x, const DRootTwo& y, long k) {
    DOmega tmp = DOmega::from_droottwo(x) + DOmega::from_droottwo(y) * ZOmega(0, 1, 0, 0);
    return tmp.renew_denomexp(k);
}

DOmega DOmega::renew_denomexp(long new_k) const {
    return DOmega(mul_by_sqrt2_power(new_k - k_).u_, new_k);
}

DOmega DOmega::reduce_denomexp() const {
    long k_a = (u_.a() == 0) ? k_ : ntz(u_.a());
    long k_b = (u_.b() == 0) ? k_ : ntz(u_.b());
    long k_c = (u_.c() == 0) ? k_ : ntz(u_.c());
    long k_d = (u_.d() == 0) ? k_ : ntz(u_.d());
    long reduce_k = std::min({k_a, k_b, k_c, k_d});
    long new_k = k_ - reduce_k * 2;
    mpz_class bit = (mpz_class(1) << static_cast<unsigned long>(reduce_k + 1)) - 1;
    if (((u_.c() + u_.a()) & bit) == 0 && ((u_.b() + u_.d()) & bit) == 0) {
        new_k -= 1;
    }
    if (new_k < 0) new_k = 0;
    return renew_denomexp(new_k);
}

DOmega DOmega::mul_by_inv_sqrt2() const {
    if (((u_.b() + u_.d()) & mpz_class(1)) != 0 ||
        ((u_.c() + u_.a()) & mpz_class(1)) != 0) {
        throw std::domain_error("DOmega::mul_by_inv_sqrt2: parity check failed");
    }
    ZOmega new_u((u_.b() - u_.d()) / 2,
                 (u_.c() + u_.a()) / 2,
                 (u_.b() + u_.d()) / 2,
                 (u_.c() - u_.a()) / 2);
    return DOmega(new_u, k_);
}

DOmega DOmega::mul_by_sqrt2_power(long d) const {
    if (d == 0) return *this;
    if (d < 0) {
        if (d == -1) return mul_by_inv_sqrt2();
        long abs_d = -d;
        long div2 = abs_d >> 1;
        long mod2 = abs_d & 1;
        if (mod2 == 0) {
            mpz_class bit = (mpz_class(1) << static_cast<unsigned long>(div2)) - 1;
            if ((u_.a() & bit) != 0 || (u_.b() & bit) != 0 ||
                (u_.c() & bit) != 0 || (u_.d() & bit) != 0) {
                throw std::domain_error("DOmega::mul_by_sqrt2_power: divisibility failed");
            }
            unsigned long shift = static_cast<unsigned long>(div2);
            ZOmega new_u(u_.a() >> shift, u_.b() >> shift,
                         u_.c() >> shift, u_.d() >> shift);
            return DOmega(new_u, k_);
        } else {
            mpz_class bit = (mpz_class(1) << static_cast<unsigned long>(div2 + 1)) - 1;
            if (((u_.b() - u_.d()) & bit) != 0 ||
                ((u_.c() + u_.a()) & bit) != 0 ||
                ((u_.b() + u_.d()) & bit) != 0 ||
                ((u_.c() - u_.a()) & bit) != 0) {
                throw std::domain_error("DOmega::mul_by_sqrt2_power: divisibility failed");
            }
            unsigned long shift = static_cast<unsigned long>(div2 + 1);
            ZOmega new_u((u_.b() - u_.d()) >> shift,
                         (u_.c() + u_.a()) >> shift,
                         (u_.b() + u_.d()) >> shift,
                         (u_.c() - u_.a()) >> shift);
            return DOmega(new_u, k_);
        }
    }
    long div2 = d >> 1;
    long mod2 = d & 1;
    mpz_class shift_amt = mpz_class(1) << static_cast<unsigned long>(div2);
    ZOmega new_u = u_ * ZOmega::from_int(shift_amt);
    if (mod2) new_u = new_u * ZOmega(-1, 0, 1, 0);
    return DOmega(new_u, k_);
}

DOmega DOmega::conj_sq2() const {
    return (k_ & 1) ? DOmega(-u_.conj_sq2(), k_)
                    : DOmega(u_.conj_sq2(), k_);
}

MPFloat DOmega::scale() const { return pow_sqrt2(k_); }
MPFloat DOmega::real() const { return u_.real() / scale(); }
MPFloat DOmega::imag() const { return u_.imag() / scale(); }

std::string DOmega::to_string() const {
    std::ostringstream os;
    os << u_.to_string() << " / √2^" << k_;
    return os.str();
}

// ============================================================
//  Constants & I/O
// ============================================================
const ZRootTwo& LAMBDA() {
    static const ZRootTwo v(1, 1);
    return v;
}
const ZOmega& OMEGA() {
    static const ZOmega v(0, 0, 1, 0);
    return v;
}
const std::array<ZOmega, 8>& OMEGA_POWER() {
    static const std::array<ZOmega, 8> v = {
        ZOmega(0, 0, 0, 1),
        ZOmega(0, 0, 1, 0),
        ZOmega(0, 1, 0, 0),
        ZOmega(1, 0, 0, 0),
        ZOmega(0, 0, 0, -1),
        ZOmega(0, 0, -1, 0),
        ZOmega(0, -1, 0, 0),
        ZOmega(-1, 0, 0, 0),
    };
    return v;
}

std::ostream& operator<<(std::ostream& os, const ZRootTwo& v) { return os << v.to_string(); }
std::ostream& operator<<(std::ostream& os, const DRootTwo& v) { return os << v.to_string(); }
std::ostream& operator<<(std::ostream& os, const ZOmega& v)   { return os << v.to_string(); }
std::ostream& operator<<(std::ostream& os, const DOmega& v)   { return os << v.to_string(); }

}  // namespace cppgridsynth
