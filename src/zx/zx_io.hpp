/**
 * @file
 * @author Design Verification Lab
 * @brief define read/write functions for ZXGraph
 *
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 *
 */

#pragma once

#include <iosfwd>

#include "./zxgraph.hpp"

namespace qsyn {

namespace zx {

std::optional<ZXGraph> from_zx(std::filesystem::path const& filename, bool keep_id = false);
std::optional<ZXGraph> from_zx(std::istream& istr, bool keep_id = false);

}  // namespace zx

}  // namespace qsyn
