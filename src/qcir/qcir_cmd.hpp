/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "argparse/argparse.hpp"

bool valid_qcir_id(size_t const& id);
bool valid_qcir_gate_id(size_t const& id);
bool valid_qcir_qubit_id(size_t const& id);
bool valid_decomposition_mode(size_t const& val);
