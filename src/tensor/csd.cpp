/****************************************************************************
  PackageName  [ tensor / csd ]
  Synopsis     [ Cosine-sine decomposition (LAPACK zuncsd). ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./csd.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

namespace qsyn::tensor::csd {

namespace {

using blasint = int;

extern "C" void zuncsd_(char* jobu1, char* jobu2, char* jobv1t, char* jobv2t, char* trans,
                        char* signs, blasint* m, blasint* p, blasint* q, double* x11,
                        blasint* ldx11, double* x12, blasint* ldx12, double* x21,
                        blasint* ldx21, double* x22, blasint* ldx22, double* theta,
                        double* u1, blasint* ldu1, double* u2, blasint* ldu2,
                        double* v1t, blasint* ldv1t, double* v2t, blasint* ldv2t,
                        double* work, blasint* lwork, double* rwork, blasint* lrwork,
                        blasint* iwork, blasint* info);

// Copy a row-major xt block into column-major storage for LAPACK (complex as pairs).
void to_col_major(std::vector<c64>& dst, cmat const& src) {
    auto const r = src.shape(0);
    auto const c = src.shape(1);
    dst.resize(r * c);
    for (size_t j = 0; j < c; ++j) {
        for (size_t i = 0; i < r; ++i) {
            dst[j * r + i] = src(i, j);
        }
    }
}

cmat from_col_major(std::vector<c64> const& src, size_t r, size_t c) {
    cmat out = xt::zeros<c64>({r, c});
    for (size_t j = 0; j < c; ++j) {
        for (size_t i = 0; i < r; ++i) {
            out(i, j) = src[j * r + i];
        }
    }
    return out;
}

}  // namespace

std::optional<CossinSeparate>
cossin_separate(cmat const& unitary) {
    if (unitary.shape(0) != unitary.shape(1) || unitary.shape(0) < 2) {
        spdlog::error("csd::cossin_separate expects a square matrix with dim >= 2.");
        return std::nullopt;
    }

    blasint m = static_cast<blasint>(unitary.shape(0));
    if ((m & 1) != 0) {
        spdlog::error("csd::cossin_separate expects even dimension, got {}.", m);
        return std::nullopt;
    }
    blasint const p = m / 2;
    blasint const q = m / 2;

    auto extract_block = [&](size_t r0, size_t r1, size_t c0, size_t c1) {
        cmat block = xt::zeros<c64>({r1 - r0, c1 - c0});
        for (size_t r = r0; r < r1; ++r)
            for (size_t c = c0; c < c1; ++c)
                block(r - r0, c - c0) = unitary(r, c);
        return block;
    };

    auto const x11 = extract_block(0, static_cast<size_t>(p), 0, static_cast<size_t>(q));
    auto const x12 = extract_block(0, static_cast<size_t>(p), static_cast<size_t>(q), static_cast<size_t>(m));
    auto const x21 = extract_block(static_cast<size_t>(p), static_cast<size_t>(m), 0, static_cast<size_t>(q));
    auto const x22 =
        extract_block(static_cast<size_t>(p), static_cast<size_t>(m), static_cast<size_t>(q), static_cast<size_t>(m));

    std::vector<c64> buf11, buf12, buf21, buf22;
    to_col_major(buf11, x11);
    to_col_major(buf12, x12);
    to_col_major(buf21, x21);
    to_col_major(buf22, x22);

    std::vector<double> theta(static_cast<size_t>(std::min({p, q, m - p, m - q})));
    std::vector<c64> u1(static_cast<size_t>(p * p));
    std::vector<c64> u2(static_cast<size_t>((m - p) * (m - p)));
    std::vector<c64> v1t(static_cast<size_t>(q * q));
    std::vector<c64> v2t(static_cast<size_t>((m - q) * (m - q)));

    char job = 'Y';
    char trans = 'N';
    char signs = 'L';

    blasint const lwork  = 32 * m;
    blasint const lrwork = 8 * m;
    std::vector<c64> work(static_cast<size_t>(lwork));
    std::vector<double> rwork(static_cast<size_t>(lrwork));
    std::vector<blasint> iwork(static_cast<size_t>(8 * m));
    blasint info = 0;

    zuncsd_(&job, &job, &job, &job, &trans, &signs, &m, const_cast<blasint*>(&p),
            const_cast<blasint*>(&q), reinterpret_cast<double*>(buf11.data()), const_cast<blasint*>(&p),
            reinterpret_cast<double*>(buf12.data()), const_cast<blasint*>(&p),
            reinterpret_cast<double*>(buf21.data()), const_cast<blasint*>(&m - p),
            reinterpret_cast<double*>(buf22.data()), const_cast<blasint*>(&m - p), theta.data(),
            reinterpret_cast<double*>(u1.data()), const_cast<blasint*>(&p),
            reinterpret_cast<double*>(u2.data()), const_cast<blasint*>(&m - p),
            reinterpret_cast<double*>(v1t.data()), const_cast<blasint*>(&q),
            reinterpret_cast<double*>(v2t.data()), const_cast<blasint*>(&m - q),
            reinterpret_cast<double*>(work.data()), const_cast<blasint*>(&lwork), rwork.data(),
            const_cast<blasint*>(&lrwork), iwork.data(), &info);

    if (info != 0) {
        spdlog::error("csd::cossin_separate: zuncsd failed (info={}).", info);
        return std::nullopt;
    }

    CossinSeparate out;
    out.u1    = from_col_major(u1, static_cast<size_t>(p), static_cast<size_t>(p));
    out.u2    = from_col_major(u2, static_cast<size_t>(m - p), static_cast<size_t>(m - p));
    out.v1h   = from_col_major(v1t, static_cast<size_t>(q), static_cast<size_t>(q));
    out.v2h   = from_col_major(v2t, static_cast<size_t>(m - q), static_cast<size_t>(m - q));
    out.theta = std::move(theta);
    return out;
}

}  // namespace qsyn::tensor::csd
