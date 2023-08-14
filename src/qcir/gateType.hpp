/****************************************************************************
  FileName     [ gateType.hpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class qcirGate structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <iosfwd>

enum class GateType {
    ID,
    // NOTE - Multi-control rotate
    MCP,
    MCRZ,
    MCPX,
    MCRX,
    MCPY,
    MCRY,
    H,
    // NOTE - MCP(Z)
    CCZ,
    CZ,
    P,
    Z,
    S,
    SDG,
    T,
    TDG,
    RZ,
    // NOTE - MCPX
    CCX,
    CX,
    SWAP,
    PX,
    X,
    SX,
    RX,
    // NOTE - MCPY
    Y,
    PY,
    SY,
    RY
};

std::ostream& operator<<(std::ostream& stream, GateType const& type);