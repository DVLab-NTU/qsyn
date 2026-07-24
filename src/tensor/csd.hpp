/****************************************************************************
  PackageName  [ tensor / csd ]
  Synopsis     [ Cosine-sine (CS) decomposition via LAPACK zuncsd. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <complex>
#include <optional>
#include <vector>
#include <xtensor/containers/xtensor.hpp>

namespace qsyn::tensor::csd {

using c64  = std::complex<double>;
using cmat = xt::xtensor<c64, 2>;

struct CossinSeparate {
    cmat u1;   // p x p
    cmat u2;   // (m-p) x (m-p)
    cmat v1h;  // q x q  (V1^dag from LAPACK V1t)
    cmat v2h;  // (m-q) x (m-q)
    std::vector<double> theta;
};

// scipy.linalg.cossin(X, p=m/2, q=m/2, separate=True) equivalent.
[[nodiscard]] std::optional<CossinSeparate>
cossin_separate(cmat const& unitary);

}  // namespace qsyn::tensor::csd
