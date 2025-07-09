/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define ZigXag URL reader for ZXGraph ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <optional>
#include <string>

#include "zx/zxgraph.hpp"

namespace qsyn::zx {

/**
 * @brief Parse ZigXag URL and convert to ZXGraph
 * 
 * ZigXag URL format: https://algassert.com/zigxag#nodes:edges
 * Nodes format: y,x,type;y,x,type;...
 * Edges format: y1,x1,y2,x2,type;y1,x1,y2,x2,type;...
 * 
 * @param zigxag_url The ZigXag URL to parse
 * @return std::optional<ZXGraph> The parsed ZXGraph, or nullopt if parsing fails
 */
std::optional<ZXGraph> from_zigxag_url(std::string const& zigxag_url);

/**
 * @brief Parse ZigXag URL string (without the https://algassert.com/zigxag# prefix)
 * 
 * @param zigxag_str The ZigXag string to parse (format: nodes:edges)
 * @return std::optional<ZXGraph> The parsed ZXGraph, or nullopt if parsing fails
 */
std::optional<ZXGraph> from_zigxag_string(std::string const& zigxag_str);

}  // namespace qsyn::zx 