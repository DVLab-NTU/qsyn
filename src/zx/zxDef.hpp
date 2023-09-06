/****************************************************************************
  FileName     [ zxDef.hpp ]
  PackageName  [ zx ]
  Synopsis     [ Define basic data or var for graph package ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <fmt/core.h>
#include <fmt/format.h>

#include <functional>
#include <iosfwd>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "util/ordered_hashmap.hpp"
#include "util/ordered_hashset.hpp"
#include "util/phase.hpp"
#include "util/textFormat.hpp"

class ZXVertex;
class ZXGraph;

enum class VertexType {
    BOUNDARY,
    Z,
    X,
    H_BOX
};

enum class EdgeType {
    SIMPLE,
    HADAMARD
};

//------------------------------------------------------------------------
//  Define types
//------------------------------------------------------------------------

using ZXVertexList = ordered_hashset<ZXVertex*>;
using EdgePair = std::pair<std::pair<ZXVertex*, ZXVertex*>, EdgeType>;
using NeighborPair = std::pair<ZXVertex*, EdgeType>;

// two boundary vertices from different ZXGraph and the edge type between them
using ZXCut = std::tuple<ZXVertex*, ZXVertex*, EdgeType>;

struct NeighborPairHash {
    size_t operator()(const NeighborPair& k) const {
        return (
            (std::hash<ZXVertex*>()(k.first) ^
             (std::hash<EdgeType>()(k.second) << 1)) >>
            1);
    }
};
struct EdgePairHash {
    size_t operator()(const EdgePair& k) const {
        return (
                   (std::hash<ZXVertex*>()(k.first.first) ^
                    (std::hash<ZXVertex*>()(k.first.second) << 1)) >>
                   1) ^
               (std::hash<EdgeType>()(k.second) << 1);
    }
};
using Neighbors = ordered_hashset<NeighborPair, NeighborPairHash>;

struct ZXCutHash {
    size_t operator()(const ZXCut& cut) const {
        auto [v1, v2, edgeType] = cut;
        // the order of v1 and v2 does not matter
        if (v1 > v2) std::swap(v1, v2);
        size_t result = std::hash<ZXVertex*>()(v1) ^ std::hash<ZXVertex*>()(v2);
        result ^= std::hash<EdgeType>()(edgeType) << 1;
        return result;
    }
};

struct ZXCutEqual {
    bool operator()(const ZXCut& lhs, const ZXCut& rhs) const {
        auto [v1, v2, edgeType] = lhs;
        auto [v3, v4, edgeType2] = rhs;
        // the order of v1 and v2 does not matter
        if (v1 > v2) std::swap(v1, v2);
        if (v3 > v4) std::swap(v3, v4);
        return v1 == v3 && v2 == v4 && edgeType == edgeType2;
    }
};

using ZXCutSet = ordered_hashset<ZXCut, ZXCutHash, ZXCutEqual>;

namespace ZXParserDetail {

struct VertexInfo {
    char type;
    int qubit;
    float column;
    std::vector<std::pair<char, size_t>> neighbors;
    Phase phase;
};

using StorageType = ordered_hashmap<size_t, VertexInfo>;

}  // namespace ZXParserDetail

//------------------------------------------------------------------------
//   Define hashes
//------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& stream, VertexType const& vt);
std::ostream& operator<<(std::ostream& stream, EdgeType const& et);

template <>
struct fmt::formatter<VertexType> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return ctx.begin();
    }
    auto format(const VertexType& vt, format_context& ctx) const -> format_context::iterator {
        using namespace dvlab;
        switch (vt) {
            case VertexType::X:
                return fmt::format_to(ctx.out(), "{}", fmt_ext::styled_if_ANSI_supported("X", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
            case VertexType::Z:
                return fmt::format_to(ctx.out(), "{}", fmt_ext::styled_if_ANSI_supported("Z", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
            case VertexType::H_BOX:
                return fmt::format_to(ctx.out(), "{}", fmt_ext::styled_if_ANSI_supported("H", fmt::fg(fmt::terminal_color::yellow) | fmt::emphasis::bold));
            case VertexType::BOUNDARY:
            default:
                return fmt::format_to(ctx.out(), "●");
        }
    }
};

template <>
struct fmt::formatter<EdgeType> {
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return ctx.begin();
    }
    auto format(const EdgeType& et, format_context& ctx) const -> format_context::iterator {
        using namespace dvlab;
        switch (et) {
            case EdgeType::HADAMARD:
                return fmt::format_to(ctx.out(), "{}", fmt_ext::styled_if_ANSI_supported("H", fmt::fg(fmt::terminal_color::blue) | fmt::emphasis::bold));
            case EdgeType::SIMPLE:
            default:
                return fmt::format_to(ctx.out(), "-");
        }
    }
};
