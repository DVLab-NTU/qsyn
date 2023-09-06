/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class qcirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <iosfwd>

enum class GateType {
    id,
    // NOTE - Multi-control rotate
    mcp,
    mcrz,
    mcpx,
    mcrx,
    mcpy,
    mcry,
    h,
    // NOTE - MCP(Z)
    ccz,
    cz,
    p,
    z,
    s,
    sdg,
    t,
    tdg,
    rz,
    // NOTE - MCPX
    ccx,
    cx,
    swap,
    px,
    x,
    sx,
    rx,
    // NOTE - MCPY
    y,
    py,
    sy,
    ry
};

std::ostream& operator<<(std::ostream& stream, GateType const& type);