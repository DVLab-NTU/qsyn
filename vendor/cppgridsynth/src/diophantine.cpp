// SPDX-License-Identifier: MIT
#include "cppgridsynth/diophantine.hpp"

#include <algorithm>
#include <random>
#include <vector>

#include "cppgridsynth/mymath.hpp"

namespace cppgridsynth {

namespace {

// Module-level RNG (matches pygridsynth's behaviour).
thread_local std::mt19937_64 g_rng(0);

mpz_class rand_int_in(const mpz_class& lo, const mpz_class& hi) {
    if (hi <= lo) return lo;
    mpz_class range = hi - lo + 1;
    // Sample uniformly in [0, range).
    mpz_class out;
    size_t bits = mpz_sizeinbase(range.get_mpz_t(), 2);
    do {
        out = 0;
        size_t remaining = bits;
        while (remaining > 0) {
            uint64_t r = g_rng();
            size_t take = std::min<size_t>(64, remaining);
            mpz_class chunk(static_cast<unsigned long>(r >> (64 - take)));
            out = (out << take) | chunk;
            remaining -= take;
        }
    } while (out >= range);
    return lo + out;
}

bool is_even(const mpz_class& n) { return mpz_even_p(n.get_mpz_t()) != 0; }

// Modular exponentiation for mpz_class.
mpz_class powm(const mpz_class& base, const mpz_class& exp, const mpz_class& mod) {
    mpz_class out;
    mpz_powm(out.get_mpz_t(), base.get_mpz_t(), exp.get_mpz_t(), mod.get_mpz_t());
    return out;
}

mpz_class gcd_mpz(const mpz_class& a, const mpz_class& b) {
    mpz_class out;
    mpz_gcd(out.get_mpz_t(), a.get_mpz_t(), b.get_mpz_t());
    return out;
}

// Brent's variant of Pollard's rho factoring.  Returns std::nullopt if
// no factor is found before the iteration / timeout budget is exhausted.
std::optional<mpz_class> find_factor(const mpz_class& n_in, LoopController& lc,
                                     long M = 128) {
    mpz_class n = n_in;
    if (is_even(n) && n > 2) return mpz_class(2);

    mpz_class a = rand_int_in(1, n);
    mpz_class y = a, r = 1, k = 0;
    long L_pow_int = static_cast<long>(mpz_sizeinbase(n.get_mpz_t(), 10));
    long L = static_cast<long>(std::pow(10.0, static_cast<double>(L_pow_int) / 4.0) * 1.1774) + 10;

    lc.start_factoring();
    while (true) {
        mpz_class x = y + n;  // unused alias kept for parity with pygridsynth
        while (k < r) {
            mpz_class q = 1;
            mpz_class y0 = y;
            for (long i = 0; i < M; ++i) {
                y = (y * y + a) % n;
                q = q * (x - y) % n;
                k += 1;
                if (k == r) break;
            }
            mpz_class g = gcd_mpz(q, n);
            if (g != 1) {
                if (g == n) {
                    y = y0;
                    for (long i = 0; i < M; ++i) {
                        y = (y * y + a) % n;
                        g = gcd_mpz(x - y, n);
                        if (g != 1) break;
                    }
                }
                if (g == n) return std::nullopt;
                return g;
            }
            if (k >= L || !lc.check_factoring_continue()) return std::nullopt;
        }
        r <<= 1;
    }
}

// Primality (Miller-Rabin) using GMP's mpz_probab_prime_p.
bool is_prime(const mpz_class& n_in, int /*L*/ = 4) {
    mpz_class n = abs(n_in);
    if (n == 0 || n == 1) return false;
    if (n == 2) return true;
    int r = mpz_probab_prime_p(n.get_mpz_t(), 25);
    return r != 0;
}

// pygridsynth's `_sqrt_negative_one`: returns h with h² ≡ -1 (mod p).
std::optional<mpz_class> sqrt_negative_one(const mpz_class& p, long L = 100) {
    for (long i = 0; i < L; ++i) {
        mpz_class b = rand_int_in(1, p - 1);
        mpz_class e = (p - 1) >> 2;
        mpz_class h = powm(b, e, p);
        mpz_class r = (h * h) % p;
        if (r == p - 1) return h;
        if (r != 1) return std::nullopt;
    }
    return std::nullopt;
}

// Lightweight F_p[X]/(X²-base) arithmetic for `_root_mod`.
struct FP2 {
    mpz_class a;
    mpz_class b;
    static thread_local mpz_class p;
    static thread_local mpz_class base;

    FP2(mpz_class A, mpz_class B) : a(std::move(A)), b(std::move(B)) {
        a = ((a % p) + p) % p;
        b = ((b % p) + p) % p;
    }
    FP2 operator*(const FP2& o) const {
        mpz_class na = (a * o.a + b * o.b * base) % p;
        mpz_class nb = (a * o.b + b * o.a) % p;
        return FP2(na, nb);
    }
    FP2 pow(mpz_class e) const {
        FP2 result(1, 0);
        FP2 base_v = *this;
        while (e > 0) {
            if (mpz_odd_p(e.get_mpz_t())) result = result * base_v;
            base_v = base_v * base_v;
            e >>= 1;
        }
        return result;
    }
};
thread_local mpz_class FP2::p(0);
thread_local mpz_class FP2::base(0);

// pygridsynth's `_root_mod`: square root of x modulo p.
std::optional<mpz_class> root_mod(const mpz_class& x_in, const mpz_class& p, long L = 100) {
    mpz_class x = ((x_in % p) + p) % p;
    if (p == 2) return x;
    if (x == 0) return mpz_class(0);
    if (is_even(p) && p > 2) return std::nullopt;
    if (powm(x, (p - 1) / 2, p) != 1) return std::nullopt;

    for (long i = 0; i < L; ++i) {
        mpz_class b = rand_int_in(1, p - 1);
        mpz_class r = powm(b, p - 1, p);
        if (r != 1) return std::nullopt;

        mpz_class base = (b * b + p - x) % p;
        if (powm(base, (p - 1) / 2, p) != 1) {
            FP2::p = p;
            FP2::base = base;
            FP2 v(b, 1);
            return v.pow((p + 1) / 2).a;
        }
    }
    return std::nullopt;
}

// ---------------- adj-decompose helpers (over Z) -----------------------
std::variant<ZOmega, DiopResult> adj_decompose_int_prime(mpz_class p) {
    if (p < 0) p = -p;
    if (p == 0 || p == 1) return ZOmega::from_int(p);
    if (p == 2) return ZOmega(-1, 0, 1, 0);

    bool prime = is_prime(p);
    if (prime) {
        mpz_class p4 = p % 4;
        mpz_class p8 = p % 8;
        long mod4 = p4.get_si();
        long mod8 = p8.get_si();
        if (mod4 == 1) {
            auto h = sqrt_negative_one(p);
            if (!h) return DiopResult::UNSOLVED;
            ZOmega t = ZOmega::gcd(ZOmega::from_int(*h) + ZOmega(0, 1, 0, 0),
                                   ZOmega::from_int(p));
            if (t.conj() * t == ZOmega::from_int(p) || t.conj() * t == -ZOmega::from_int(p)) return t;
            return DiopResult::UNSOLVED;
        }
        if (mod8 == 3) {
            auto h = root_mod(-2, p);
            if (!h) return DiopResult::UNSOLVED;
            ZOmega t = ZOmega::gcd(ZOmega::from_int(*h) + ZOmega(1, 0, 1, 0),
                                   ZOmega::from_int(p));
            if (t.conj() * t == ZOmega::from_int(p) || t.conj() * t == -ZOmega::from_int(p)) return t;
            return DiopResult::UNSOLVED;
        }
        if (mod8 == 7) {
            auto h = root_mod(2, p);
            if (h) return DiopResult::NO_SOLUTION;
            return DiopResult::UNSOLVED;
        }
        return DiopResult::UNSOLVED;
    }
    mpz_class p8 = p % 8;
    long mod8 = p8.get_si();
    if (mod8 == 7) {
        auto h = root_mod(2, p);
        if (h) return DiopResult::NO_SOLUTION;
        return DiopResult::UNSOLVED;
    }
    return DiopResult::UNSOLVED;
}

std::variant<ZOmega, DiopResult> adj_decompose_int_prime_power(const mpz_class& p, long k) {
    if ((k & 1) == 0) {
        mpz_class v;
        mpz_pow_ui(v.get_mpz_t(), p.get_mpz_t(), static_cast<unsigned long>(k / 2));
        return ZOmega::from_int(v);
    }
    auto t = adj_decompose_int_prime(p);
    if (std::holds_alternative<DiopResult>(t)) return std::get<DiopResult>(t);
    ZOmega base = std::get<ZOmega>(t);
    return base.pow(static_cast<unsigned long>(k));
}

std::pair<mpz_class, std::vector<std::pair<mpz_class, long>>>
decompose_relatively_int_prime(std::vector<std::pair<mpz_class, long>> partial_facs) {
    mpz_class u = 1;
    std::vector<std::pair<mpz_class, long>> stack(partial_facs.rbegin(), partial_facs.rend());
    std::vector<std::pair<mpz_class, long>> facs;
    while (!stack.empty()) {
        auto [b, k_b] = stack.back();
        stack.pop_back();
        size_t i = 0;
        while (true) {
            if (i >= facs.size()) {
                if (b == 1 || b == -1) {
                    if (b == -1 && (k_b & 1)) u = -u;
                } else {
                    facs.emplace_back(b, k_b);
                }
                break;
            }
            auto [a, k_a] = facs[i];
            if (a == b || a == -b) {
                if (a == -b && (k_b & 1)) u = -u;
                facs[i] = {a, k_a + k_b};
                break;
            }
            mpz_class g = gcd_mpz(a, b);
            if (g == 1 || g == -1) { ++i; continue; }
            std::vector<std::pair<mpz_class, long>> sub = {
                {a / g, k_a},
                {g, k_a + k_b}
            };
            auto [u_a, facs_a] = decompose_relatively_int_prime(sub);
            u *= u_a;
            facs[i] = facs_a[0];
            for (size_t j = 1; j < facs_a.size(); ++j) facs.push_back(facs_a[j]);
            stack.emplace_back(b / g, k_b);
            break;
        }
    }
    return {u, facs};
}

std::variant<ZOmega, DiopResult> adj_decompose_int(mpz_class n, LoopController& lc) {
    if (n < 0) n = -n;
    std::vector<std::pair<mpz_class, long>> facs = {{n, 1}};
    ZOmega t = ZOmega::from_int(1);
    while (!facs.empty()) {
        auto [p, k] = facs.back();
        facs.pop_back();
        auto t_p = adj_decompose_int_prime_power(p, k);
        if (std::holds_alternative<DiopResult>(t_p)) {
            DiopResult r = std::get<DiopResult>(t_p);
            if (r == DiopResult::NO_SOLUTION) return DiopResult::NO_SOLUTION;
            // UNSOLVED: try factoring p further.
            auto fac = find_factor(p, lc);
            if (!fac) {
                facs.emplace_back(p, k);
                if (!lc.check_diophantine_continue()) return DiopResult::NO_SOLUTION;
            } else {
                facs.emplace_back(p / *fac, k);
                facs.emplace_back(*fac, k);
                auto [u_a, new_facs] = decompose_relatively_int_prime(facs);
                (void)u_a;
                facs = new_facs;
            }
        } else {
            t = t * std::get<ZOmega>(t_p);
        }
    }
    return t;
}

// ---------------- adj-decompose helpers (over Z[√2]) -------------------
std::variant<ZOmega, DiopResult> adj_decompose_zomega_prime(const ZRootTwo& eta) {
    mpz_class p = eta.norm();
    if (p < 0) p = -p;
    if (p == 0 || p == 1) return ZOmega::from_int(p);
    if (p == 2) return ZOmega(-1, 0, 1, 0);

    bool prime = is_prime(p);
    if (prime) {
        mpz_class p4 = p % 4;
        mpz_class p8 = p % 8;
        long mod4 = p4.get_si();
        long mod8 = p8.get_si();
        if (mod4 == 1) {
            auto h = sqrt_negative_one(p);
            if (!h) return DiopResult::UNSOLVED;
            ZOmega t = ZOmega::gcd(ZOmega::from_int(*h) + ZOmega(0, 1, 0, 0),
                                   ZOmega::from_zroottwo(eta));
            if (ZOmega::sim(t.conj() * t, ZOmega::from_zroottwo(eta))) return t;
            return DiopResult::UNSOLVED;
        }
        if (mod8 == 3) {
            auto h = root_mod(-2, p);
            if (!h) return DiopResult::UNSOLVED;
            ZOmega t = ZOmega::gcd(ZOmega::from_int(*h) + ZOmega(1, 0, 1, 0),
                                   ZOmega::from_zroottwo(eta));
            if (ZOmega::sim(t.conj() * t, ZOmega::from_zroottwo(eta))) return t;
            return DiopResult::UNSOLVED;
        }
        if (mod8 == 7) {
            auto h = root_mod(2, p);
            if (h) return DiopResult::NO_SOLUTION;
            return DiopResult::UNSOLVED;
        }
        return DiopResult::UNSOLVED;
    }
    mpz_class p8 = p % 8;
    long mod8 = p8.get_si();
    if (mod8 == 7) {
        auto h = root_mod(2, p);
        if (h) return DiopResult::NO_SOLUTION;
        return DiopResult::UNSOLVED;
    }
    return DiopResult::UNSOLVED;
}

std::variant<ZOmega, DiopResult>
adj_decompose_zomega_prime_power(const ZRootTwo& eta, long k) {
    if ((k & 1) == 0) {
        return ZOmega::from_zroottwo(eta.pow(k / 2));
    }
    auto t = adj_decompose_zomega_prime(eta);
    if (std::holds_alternative<DiopResult>(t)) return std::get<DiopResult>(t);
    ZOmega base = std::get<ZOmega>(t);
    return base.pow(static_cast<unsigned long>(k));
}

std::pair<ZRootTwo, std::vector<std::pair<ZRootTwo, long>>>
decompose_relatively_zomega_prime(std::vector<std::pair<ZRootTwo, long>> partial_facs) {
    ZRootTwo u = ZRootTwo::from_int(1);
    std::vector<std::pair<ZRootTwo, long>> stack(partial_facs.rbegin(), partial_facs.rend());
    std::vector<std::pair<ZRootTwo, long>> facs;
    while (!stack.empty()) {
        auto [b, k_b] = stack.back();
        stack.pop_back();
        size_t i = 0;
        while (true) {
            if (i >= facs.size()) {
                if (ZRootTwo::sim(b, ZRootTwo::from_int(1))) {
                    u *= b.pow(k_b);
                } else {
                    facs.emplace_back(b, k_b);
                }
                break;
            }
            auto [a, k_a] = facs[i];
            if (ZRootTwo::sim(a, b)) {
                u *= b.floordiv(a).pow(k_b);
                facs[i] = {a, k_a + k_b};
                break;
            }
            ZRootTwo g = ZRootTwo::gcd(a, b);
            if (ZRootTwo::sim(g, ZRootTwo::from_int(1))) { ++i; continue; }
            std::vector<std::pair<ZRootTwo, long>> sub = {
                {a.floordiv(g), k_a},
                {g, k_a + k_b}
            };
            auto [u_a, facs_a] = decompose_relatively_zomega_prime(sub);
            u *= u_a;
            facs[i] = facs_a[0];
            for (size_t j = 1; j < facs_a.size(); ++j) facs.push_back(facs_a[j]);
            stack.emplace_back(b.floordiv(g), k_b);
            break;
        }
    }
    return {u, facs};
}

std::variant<ZOmega, DiopResult> adj_decompose_selfassociate(const ZRootTwo& xi,
                                                             LoopController& lc) {
    if (xi == ZRootTwo::from_int(0)) return ZOmega::from_int(0);

    mpz_class n = gcd_mpz(xi.a(), xi.b());
    ZRootTwo r = xi.floordiv(ZRootTwo::from_int(n));
    auto t1 = adj_decompose_int(n, lc);
    ZOmega t2 = (r.mod(ZRootTwo(0, 1)) == ZRootTwo::from_int(0))
                    ? ZOmega(0, 0, 1, 1) : ZOmega::from_int(1);
    if (std::holds_alternative<DiopResult>(t1)) return DiopResult::NO_SOLUTION;
    return std::get<ZOmega>(t1) * t2;
}

std::variant<ZOmega, DiopResult>
adj_decompose_selfcoprime(const ZRootTwo& xi, LoopController& lc) {
    std::vector<std::pair<ZRootTwo, long>> facs = {{xi, 1}};
    ZOmega t = ZOmega::from_int(1);
    while (!facs.empty()) {
        auto [eta, k] = facs.back();
        facs.pop_back();
        auto t_eta = adj_decompose_zomega_prime_power(eta, k);
        if (std::holds_alternative<DiopResult>(t_eta)) {
            DiopResult r = std::get<DiopResult>(t_eta);
            if (r == DiopResult::NO_SOLUTION) return DiopResult::NO_SOLUTION;
            mpz_class n = eta.norm();
            if (n < 0) n = -n;
            auto fac_n = find_factor(n, lc);
            if (!fac_n) {
                facs.emplace_back(eta, k);
                if (!lc.check_diophantine_continue()) return DiopResult::NO_SOLUTION;
            } else {
                ZRootTwo fac = ZRootTwo::gcd(xi, ZRootTwo::from_int(*fac_n));
                facs.emplace_back(eta.floordiv(fac), k);
                facs.emplace_back(fac, k);
                auto [u_a, new_facs] = decompose_relatively_zomega_prime(facs);
                (void)u_a;
                facs = new_facs;
            }
        } else {
            t = t * std::get<ZOmega>(t_eta);
        }
    }
    return t;
}

std::variant<ZOmega, DiopResult> adj_decompose(const ZRootTwo& xi, LoopController& lc) {
    if (xi == ZRootTwo::from_int(0)) return ZOmega::from_int(0);
    ZRootTwo d = ZRootTwo::gcd(xi, xi.conj_sq2());
    ZRootTwo eta = xi.floordiv(d);
    auto t1 = adj_decompose_selfassociate(d, lc);
    if (std::holds_alternative<DiopResult>(t1)) return DiopResult::NO_SOLUTION;
    auto t2 = adj_decompose_selfcoprime(eta, lc);
    if (std::holds_alternative<DiopResult>(t2)) return DiopResult::NO_SOLUTION;
    return std::get<ZOmega>(t1) * std::get<ZOmega>(t2);
}

std::variant<ZOmega, DiopResult> diophantine(const ZRootTwo& xi, LoopController& lc) {
    lc.start_diophantine();
    if (xi == ZRootTwo::from_int(0)) return ZOmega::from_int(0);
    if (xi < ZRootTwo::from_int(0) || xi.conj_sq2() < ZRootTwo::from_int(0))
        return DiopResult::NO_SOLUTION;

    auto t = adj_decompose(xi, lc);
    if (std::holds_alternative<DiopResult>(t)) return DiopResult::NO_SOLUTION;
    ZOmega tv = std::get<ZOmega>(t);
    ZRootTwo xi_associate = ZRootTwo::from_zomega(tv.conj() * tv);
    ZRootTwo u = xi.floordiv(xi_associate);
    auto v = u.sqrt();
    if (!v) return DiopResult::NO_SOLUTION;
    return ZOmega::from_zroottwo(*v) * tv;
}

}  // namespace

void set_random_seed(unsigned long seed) {
    g_rng.seed(seed);
}

std::variant<DOmega, DiopResult> diophantine_dyadic(const DRootTwo& xi,
                                                    int seed,
                                                    LoopController& lc) {
    set_random_seed(static_cast<unsigned long>(seed));

    long k_div_2 = xi.k() >> 1;
    long k_mod_2 = xi.k() & 1;
    ZRootTwo argument = k_mod_2 ? xi.alpha() * ZRootTwo(1, 1) : xi.alpha();

    auto t = diophantine(argument, lc);
    if (std::holds_alternative<DiopResult>(t)) return DiopResult::NO_SOLUTION;
    ZOmega tv = std::get<ZOmega>(t);
    if (k_mod_2) tv = tv * ZOmega(0, -1, 1, 0);
    return DOmega(tv, k_div_2 + k_mod_2);
}

}  // namespace cppgridsynth
